// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file ProgramFlash.cxx
/// \brief Utility that flashes the card
///
/// \author Pascal Boeschoten (pascal.boeschoten@cern.ch)

#include "CommandLineUtilities/Program.h"
#include <iostream>
#include <string>
#include "ReadoutCard/ChannelFactory.h"
#include "Crorc/Crorc.h"
#include "ExceptionInternal.h"

using namespace AliceO2::roc::CommandLineUtilities;
using std::cout;
using std::endl;
namespace po = boost::program_options;

namespace
{
class ProgramCrorcFlash : public Program
{
 public:
  virtual Description getDescription()
  {
    return { "Flash", "Programs the card's flash memory", "roc-flash --id=12345 --file=/dir/my_file" };
  }

  virtual void addOptions(po::options_description& options)
  {
    Options::addOptionCardId(options);
    options.add_options()("file", po::value<std::string>(&mFilePath)->required(), "Path of file to flash");
  }

  virtual void run(const boost::program_options::variables_map& map)
  {
    using namespace AliceO2::roc;

    auto cardId = Options::getOptionCardId(map);
    auto channelNumber = 0;
    auto params = AliceO2::roc::Parameters::makeParameters(cardId, channelNumber);
    auto channel = AliceO2::roc::ChannelFactory().getBar(params);

    if (channel->getCardType() != CardType::Crorc) {
      BOOST_THROW_EXCEPTION(Exception() << ErrorInfo::Message("Only C-RORC supported for now"));
    }

    Crorc::programFlash(*(channel.get()), mFilePath, 0, std::cout, &Program::getInterruptFlag());
  }

  std::string mFilePath;
};
} // Anonymous namespace

int main(int argc, char** argv)
{
  return ProgramCrorcFlash().execute(argc, argv);
}
