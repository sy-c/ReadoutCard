// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file ProgramReset.cxx
/// \brief Utility that resets a ReadoutCard
///
/// \author Pascal Boeschoten (pascal.boeschoten@cern.ch)

#include "CommandLineUtilities/Program.h"
#include <iostream>
#include "ReadoutCard/ChannelFactory.h"
#include "ReadoutCard/MemoryMappedFile.h"

namespace
{
using namespace AliceO2::roc::CommandLineUtilities;

class ProgramReset : public Program
{
 public:
  virtual Description getDescription()
  {
    return { "Reset", "Resets a channel", "roc-reset --id=12345 --channel=0 --reset=INTERNAL_DIU_SIU" };
  }

  virtual void addOptions(boost::program_options::options_description& options)
  {
    Options::addOptionRegisterAddress(options);
    Options::addOptionChannel(options);
    Options::addOptionCardId(options);
    Options::addOptionResetLevel(options);
  }

  virtual void run(const boost::program_options::variables_map& map)
  {
    auto resetLevel = Options::getOptionResetLevel(map);
    auto cardId = Options::getOptionCardId(map);
    int channelNumber = Options::getOptionChannel(map);

    auto params = AliceO2::roc::Parameters::makeParameters(cardId, channelNumber);
    params.setBufferParameters(AliceO2::roc::buffer_parameters::Null());
    auto channel = AliceO2::roc::ChannelFactory().getDmaChannel(params);
    channel->resetChannel(resetLevel);
  }
};
} // Anonymous namespace

int main(int argc, char** argv)
{
  return ProgramReset().execute(argc, argv);
}
