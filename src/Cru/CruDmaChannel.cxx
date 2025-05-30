
// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
/// \file CruDmaChannel.cxx
/// \brief Implementation of the CruDmaChannel class.
///
/// \author Pascal Boeschoten (pascal.boeschoten@cern.ch)
/// \author Kostas Alexopoulos (kostas.alexopoulos@cern.ch)

#include <thread>
#include <boost/format.hpp>
#include "CruDmaChannel.h"
#include "DatapathWrapper.h"
#include "ExceptionInternal.h"
#include "ReadoutCard/ChannelFactory.h"

using namespace std::literals;
using boost::format;

namespace o2
{
namespace roc
{

CruDmaChannel::CruDmaChannel(const Parameters& parameters)
  : DmaChannelPdaBase(parameters, allowedChannels()),
    mDataSource(parameters.getDataSource().get_value_or(DataSource::Internal)), // DG loopback mode by default
    mDmaPageSize(parameters.getDmaPageSize().get_value_or(Cru::DMA_PAGE_SIZE))
{

  if (auto pageSize = parameters.getDmaPageSize()) {
    if (pageSize.get() != Cru::DMA_PAGE_SIZE) {
      log("DMA page size not default; Behaviour undefined", LogWarningDevel_(4250));
      /*BOOST_THROW_EXCEPTION(CruException()
          << ErrorInfo::Message(getLoggerPrefix() + "CRU only supports an 8KiB page size")
          << ErrorInfo::DmaPageSize(pageSize.get()));*/
    }
  }

  if (mDataSource == DataSource::Diu || mDataSource == DataSource::Siu) {
    BOOST_THROW_EXCEPTION(CruException() << ErrorInfo::Message(getLoggerPrefix() + "CRU does not support specified data source")
                                         << ErrorInfo::DataSource(mDataSource));
  }

  // Prep for BARs
  auto parameters2 = parameters;
  parameters2.setChannelNumber(2);
  auto bar = ChannelFactory().getBar(parameters);
  auto bar2 = ChannelFactory().getBar(parameters2);
  cruBar = std::move(std::dynamic_pointer_cast<CruBar>(bar));   // Initialize BAR 0
  cruBar2 = std::move(std::dynamic_pointer_cast<CruBar>(bar2)); // Initialize BAR 2
  mFeatures = getBar()->getFirmwareFeatures();                  // Get which features of the firmware are enabled

  if (mFeatures.standalone) { //TODO: ??
    std::stringstream stream;
    auto logFeature = [&](auto name, bool enabled) { if (!enabled) { stream << " " << name; } };
    stream << "Standalone firmware features disabled:";
    logFeature("firmware-info", mFeatures.firmwareInfo);
    logFeature("serial-number", mFeatures.serial);
    logFeature("temperature", mFeatures.temperature);
    logFeature("data-selection", mFeatures.dataSelection);
    log(stream.str(), LogDebugDevel_(4251));
  }

  // Calculate link and ready queue capacities
  {
    uint32_t maxSuperpageDescriptors = getBar()->getMaxSuperpageDescriptors();
    // If number of descriptors reported is 0, the feature is not supported by the firmware. Fall back to default value
    if (maxSuperpageDescriptors == 0x0) {
      maxSuperpageDescriptors = Cru::MAX_SUPERPAGE_DESCRIPTORS_DEFAULT;
    }

    mLinkQueueCapacity = maxSuperpageDescriptors;
    mReadyQueueCapacity = maxSuperpageDescriptors * Cru::MAX_LINKS;
  }

  // Insert links
  {
    std::stringstream stream;
    stream << "Using link(s): ";

    auto links = getBar2()->getDataTakingLinks();
    for (auto const& link : links) {
      stream << link << " ";
      //Implicit constructors are deleted for the folly Queue. Workaround to keep the Link struct with a queue * field.
      std::shared_ptr<SuperpageQueue> linkQueue = std::make_shared<SuperpageQueue>(mLinkQueueCapacity + 1); // folly queue needs + 1
      Link newLink = { static_cast<LinkId>(link), 0, linkQueue };
      mLinks.push_back(newLink);
    }

    log(stream.str(), LogInfoDevel_(4252));

    if (mLinks.empty()) {
      BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "No links are enabled. Check with roc-status. Configure with roc-config."));
    }

    mReadyQueue = std::make_unique<SuperpageQueue>(mReadyQueueCapacity + 1); // folly queue needs + 1
  }
}

auto CruDmaChannel::allowedChannels() -> AllowedChannels
{
  // We have only one DMA channel per CRU endpoint
  return { 0 };
}

CruDmaChannel::~CruDmaChannel()
{
  setBufferNonReady();
  if (mReadyQueue->sizeGuess() > 0) {
    log((format("Remaining superpages in the ready queue: %1%") % mReadyQueue->sizeGuess()).str(), LogDebugDevel_(4253));
  }

  if (mDataSource == DataSource::Internal) {
    resetDebugMode();
  }
}

void CruDmaChannel::deviceStartDma()
{
  // Set data source
  uint32_t dataSourceSelection = 0x0;

  if (mDataSource == DataSource::Internal) {
    enableDebugMode();
    dataSourceSelection = Cru::Registers::DATA_SOURCE_SELECT_INTERNAL;
  } else { // data source == FEE or DDG
    dataSourceSelection = Cru::Registers::DATA_SOURCE_SELECT_GBT;
  }

  if (mFeatures.dataSelection) {
    getBar()->setDataSource(dataSourceSelection);
  } else {
    log("Did not set data source, feature not supported by firmware", LogWarningDevel_(4254));
  }

  if (dataSourceSelection == Cru::Registers::DATA_SOURCE_SELECT_GBT) {
    getBar2()->disableDataTaking(); // Make sure we don't start from a bad state - shall be done before reset
  }

  // Reset CRU (should be done after link mask set)
  resetCru();

  // Initialize link queues
  for (auto& link : mLinks) {
    while (!link.queue->isEmpty()) {
      link.queue->popFront();
    }
    link.superpageCounter = 0;
  }
  while (!mReadyQueue->isEmpty()) {
    mReadyQueue->popFront();
  }
  mLinkQueuesTotalAvailable = mLinkQueueCapacity * mLinks.size();

  // Start DMA
  setBufferReady();

  // Enable data taking
  if (dataSourceSelection == Cru::Registers::DATA_SOURCE_SELECT_GBT) {
    getBar2()->enableDataTaking();
  }
}

/// Set buffer to ready
void CruDmaChannel::setBufferReady()
{
  getBar()->startDmaEngine();
  std::this_thread::sleep_for(10ms);
}

/// Set buffer to non-ready
void CruDmaChannel::setBufferNonReady()
{
  getBar()->stopDmaEngine();
}

void CruDmaChannel::deviceStopDma()
{
  // Disable data taking
  setBufferNonReady();
  getBar2()->disableDataTaking();

  // Transfer remaining (filled) superpages to ReadyQueue
  fillSuperpages();

  // Return any superpages that have been pushed up in the meantime but won't get filled
  reclaimSuperpages();
}

void CruDmaChannel::reclaimSuperpages()
{
  for (auto& link : mLinks) {
    while (!link.queue->isEmpty()) {
      transferSuperpageFromLinkToReady(link, true); // Reclaim pages, do *not* set as ready
    }

    if (!link.queue->isEmpty()) {
      log((format("Superpage queue of link %1% not empty after DMA stop. Superpages unclaimed.") % link.id).str(), LogErrorDevel_(4255));
    }
  }
}

void CruDmaChannel::deviceResetChannel(ResetLevel::type resetLevel)
{
  if (resetLevel == ResetLevel::Nothing) {
    return;
  } else if (resetLevel != ResetLevel::Internal) {
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "The CRU can only be reset internally"));
  }
  resetCru();
}

CardType::type CruDmaChannel::getCardType()
{
  return CardType::Cru;
}

void CruDmaChannel::resetCru()
{
  getBar()->resetDataGeneratorCounter();
  std::this_thread::sleep_for(100ms);
  getBar()->resetCard();
  std::this_thread::sleep_for(100ms);
  getBar()->resetInternalCounters();
}

auto CruDmaChannel::getNextLinkIndex() -> LinkIndex
{
  auto smallestQueueIndex = std::numeric_limits<LinkIndex>::max();
  auto smallestQueueSize = std::numeric_limits<size_t>::max();

  for (size_t i = 0; i < mLinks.size(); ++i) {
    auto queueSize = mLinks[i].queue->sizeGuess();
    if (queueSize < smallestQueueSize) {
      smallestQueueIndex = i;
      smallestQueueSize = queueSize;
    }
  }

  return smallestQueueIndex;
}

bool CruDmaChannel::pushSuperpage(Superpage superpage)
{
  if (mDmaState != DmaState::STARTED) {
    return false;
  }

  checkSuperpage(superpage);

  if (mLinkQueuesTotalAvailable == 0) {
    // Note: the transfer queue refers to the firmware, not the mLinkIndexQueue which contains the LinkIds for links
    // that can still be pushed into (essentially the opposite of the firmware's queue).
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "Could not push superpage, transfer queue was full"));
  }

  // Get the next link to push
  auto& link = mLinks[getNextLinkIndex()];

  if (link.queue->sizeGuess() >= mLinkQueueCapacity) {
    // Is the link's FIFO out of space?
    // This should never happen
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "Could not push superpage, link queue was full"));
  }

  // Once we've confirmed the link has a slot available, we push the superpage
  pushSuperpageToLink(link, superpage);
  auto dmaPages = superpage.getSize() / mDmaPageSize;
  auto busAddress = getBusOffsetAddress(superpage.getOffset());
  getBar()->pushSuperpageDescriptor(link.id, dmaPages, busAddress);

  mFirstSPPushed = true;

  return true;
}

auto CruDmaChannel::getSuperpage() -> Superpage
{
  if (mReadyQueue->isEmpty()) {
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "Could not get superpage, ready queue was empty"));
  }
  return *mReadyQueue->frontPtr();
}

auto CruDmaChannel::popSuperpage() -> Superpage
{
  if (mReadyQueue->isEmpty()) {
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "Could not pop superpage, ready queue was empty"));
  }
  auto superpage = *mReadyQueue->frontPtr();
  mReadyQueue->popFront();

  return superpage;
}

void CruDmaChannel::pushSuperpageToLink(Link& link, const Superpage& superpage)
{
  mLinkQueuesTotalAvailable--;
  link.queue->write(superpage);
}

void CruDmaChannel::transferSuperpageFromLinkToReady(Link& link, bool reclaim)
{
  if (link.queue->isEmpty()) {
    BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message(getLoggerPrefix() + "Could not transfer Superpage from link to ready queue, link queue is empty"));
  }

  if (!reclaim) {
    link.queue->frontPtr()->setReady(true);
    uint32_t superpageSize = getBar()->getSuperpageSize(link.id);
    if (superpageSize == 0) {
      link.queue->frontPtr()->setReceived(link.queue->frontPtr()->getSize()); // force the full superpage size for backwards compatibility
    } else {
      link.queue->frontPtr()->setReceived(superpageSize);
    }
  } else {
    link.queue->frontPtr()->setReady(false);
    link.queue->frontPtr()->setReceived(0);
  }

  link.queue->frontPtr()->setLink(link.id);
  mReadyQueue->write(*link.queue->frontPtr());
  link.queue->popFront();
  link.superpageCounter++;
  mLinkQueuesTotalAvailable++;
}

void CruDmaChannel::fillSuperpages()
{

  /*if (mDmaState != DmaState::STARTED) { //Would block fillSuperpages from deviceStopDma()
    //return;
  }*/

  // Check for arrivals & handle them
  for (auto& link : mLinks) {
    int32_t superpageCount = getBar()->getSuperpageCount(link.id);
    uint32_t amountAvailable = superpageCount - link.superpageCounter;
    if (amountAvailable > link.queue->sizeGuess()) {

      std::stringstream stream;
      stream << "FATAL: Firmware reported more superpages available (" << amountAvailable << ") than should be present in FIFO (" << link.queue->sizeGuess() << "); "
             << link.superpageCounter << " superpages received from link " << int(link.id) << " according to driver, "
             << superpageCount << " pushed according to firmware";
      log(stream.str(), LogErrorDevel_(4256));
      BOOST_THROW_EXCEPTION(Exception()
                            << ErrorInfo::Message(getLoggerPrefix() + "FATAL: Firmware reported more superpages available than should be present in FIFO"));
    }

    while (amountAvailable) {
      if (mReadyQueue->sizeGuess() >= mReadyQueueCapacity) {
        break;
      }

      transferSuperpageFromLinkToReady(link);
      amountAvailable--;
    }
  }
}

int CruDmaChannel::getTransferQueueAvailable()
{
  return mLinkQueuesTotalAvailable;
}

// Return a boolean that denotes whether the transfer queue is empty
// The transfer queue is empty when all its slots are available
bool CruDmaChannel::isTransferQueueEmpty()
{
  return mLinkQueuesTotalAvailable == (mLinkQueueCapacity * mLinks.size());
}

int CruDmaChannel::getReadyQueueSize()
{
  return mReadyQueue->sizeGuess();
}

// Return a boolean that denotes whether the ready queue is full
// The ready queue is full when the CRU has filled it up
bool CruDmaChannel::isReadyQueueFull()
{
  return mReadyQueue->isFull();
}

int32_t CruDmaChannel::getDroppedPackets()
{
  int endpoint = getBar()->getEndpointNumber();
  return getBar2()->getDroppedPackets(endpoint);
}

bool CruDmaChannel::areSuperpageFifosHealthy()
{
  if (mDmaState != DmaState::STARTED || !mFirstSPPushed) {
    return true;
  }

  bool ok = true;

  static ILAutoMuteToken logToken(LogWarningDevel_(4257), 15, 60);

  for (const auto& link : mLinks) {
    uint32_t emptyCounter = getBar()->getSuperpageFifoEmptyCounter(link.id);
    if (mEmptySPFifoCounters.count(link.id) && //only check after the counters map has been initialized
        mEmptySPFifoCounters[link.id] != emptyCounter) {
      Logger::get().log(logToken, "%s", (mLoggerPrefix + (format("Empty counter of Superpage FIFO of link %d increased from %x to %x") % link.id % mEmptySPFifoCounters[link.id] % emptyCounter).str()).c_str());
      ok = false;
    }
    mEmptySPFifoCounters[link.id] = emptyCounter;
  }

  return ok;
}

bool CruDmaChannel::injectError()
{
  if (!mDataSource == DataSource::Fee) {
    getBar()->dataGeneratorInjectError();
    return true;
  } else {
    return false;
  }
}

void CruDmaChannel::enableDebugMode()
{
  if (!getBar()->getDebugModeEnabled()) {
    getBar()->setDebugModeEnabled(true);
    mDebugRegisterReset = true;
  }
}

void CruDmaChannel::resetDebugMode()
{
  if (mDebugRegisterReset) {
    getBar()->setDebugModeEnabled(false);
  }
}

boost::optional<int32_t> CruDmaChannel::getSerial()
{
  if (mFeatures.serial) {
    return getBar2()->getSerial();
  } else {
    return {};
  }
}

boost::optional<float> CruDmaChannel::getTemperature()
{
  if (mFeatures.temperature) {
    return getBar2()->getTemperature();
  } else {
    return {};
  }
}

boost::optional<std::string> CruDmaChannel::getFirmwareInfo()
{
  if (mFeatures.firmwareInfo) {
    return getBar2()->getFirmwareInfo();
  } else {
    return {};
  }
}

boost::optional<std::string> CruDmaChannel::getCardId()
{
  if (mFeatures.chipId) {
    return getBar2()->getCardId();
  } else {
    return {};
  }
}

int32_t CruDmaChannel::getCounterFirstOrbit() {
  int address = 0x0;
  int endpoint = getBar()->getEndpointNumber();
  if (endpoint == 0) {
    address = 0x64002C;
  } else if (endpoint == 1) {
    address = 0x74002C;
  } else {
    return -1;
  }
  return getBar2()->readRegister(address / 4);
}

} // namespace roc
} // namespace o2
