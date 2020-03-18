//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
//
// ### Software developed externally (not by the QFlex group)
//
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
//
// ### Software developed internally (by the QFlex group)
// **QFlex License**
//
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include <boost/optional.hpp>
#include <core/boost_extensions/lexical_cast.hpp>

#include <core/target.hpp>

// FIXME need to make sure that CONFIG_QEMU actually works
#ifndef CONFIG_QEMU
#define CONFIG_QEMU
#endif

#include <core/flexus.hpp>
#include <core/stats.hpp>

#include <core/boost_extensions/circular_buffer.hpp>

#include <core/debug/action.hpp>
#include <core/debug/debugger.hpp>

namespace Flexus {

namespace Core {
void Break();
}

namespace Dbg {

void CompoundAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  for (auto *anAction : theActions) {
    anAction->printConfiguration(anOstream, anIndent);
  }
}

void CompoundAction::process(Entry const &anEntry) {
  for (auto *anAction : theActions) {
    anAction->process(anEntry);
  }
}

CompoundAction::~CompoundAction() {
  for (auto *anAction : theActions) {
    delete anAction;
  }
}

void CompoundAction::add(std::unique_ptr<Action> anAction) {
  theActions.push_back(anAction.release()); // Ownership assumed by theActions
}

void ConsoleLogAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "log console";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void ConsoleLogAction::process(Entry const &anEntry) {
  theFormat->format(std::cerr, anEntry);
  std::cerr.flush();
}

class StreamManager {
  typedef std::map<std::string, std::ofstream *> stream_map;
  stream_map theStreams;

public:
  std::ostream &getStream(std::string const &aFile) {
    std::pair<stream_map::iterator, bool> insert_result;
    insert_result = theStreams.insert(std::make_pair(aFile, static_cast<std::ofstream *>(0)));
    if (insert_result.second) {
      (*insert_result.first).second = new std::ofstream(aFile.c_str());
      std::cout << "Opening debug output file: " << aFile << "\n";
    }
    return *(*insert_result.first).second;
  }

  ~StreamManager() {
    stream_map::iterator iter = theStreams.begin();
    while (iter != theStreams.end()) {
      (*iter).second->close();
      delete (*iter).second;
      ++iter;
    }
  }
};

// The StreamManager instance
std::unique_ptr<StreamManager> theStreamManager;

// StreamManager accessor
inline StreamManager &streamManager() {
  if (theStreamManager.get() == 0) {
    theStreamManager.reset(new StreamManager());
  }
  return *theStreamManager;
}

FileLogAction::FileLogAction(std::string aFilename, Format *aFormat)
    : theFilename(aFilename), theOstream(streamManager().getStream(aFilename)),
      theFormat(aFormat){};

void FileLogAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "log " << theFilename;
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void FileLogAction::process(Entry const &anEntry) {
  theFormat->format(theOstream, anEntry);
  theOstream.flush();
}

void AbortAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "abort ;";
}

void AbortAction::process(Entry const &anEntry) {
  std::abort();
}

void BreakAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "break ;";
}

void BreakAction::process(Entry const &anEntry) {
  Flexus::Core::Break();
}

void PrintStatsAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "print-stats ;";
}

void PrintStatsAction::process(Entry const &anEntry) {
  Flexus::Stat::getStatManager()->printMeasurement("all", std::cout);
}

SeverityAction::SeverityAction(uint32_t aSeverity) : theSeverity(aSeverity){};

void SeverityAction::printConfiguration(std::ostream &anOstream, std::string const &anIndent) {
  anOstream << anIndent << "set-global-severity " << toString(Severity(theSeverity)) << " ;";
}

void SeverityAction::process(Entry const &anEntry) {
  Flexus::Dbg::Debugger::theDebugger->setMinSev(Severity(theSeverity));
}

#if 0
class SaveBufferManager {
  typedef boost::circular_buffer< Entry const & > entry_buffer;
  typedef std::map<std::string, std::shared_ptr< entry_buffer > > buf_map;
  buf_map theBuffers;
public:
  //Need destructor

  void create(std::string const & aBufferName, uint32_t aSize) {
    if (theBuffers[aBufferName].get() == 0) {
      theBuffers[aBufferName].reset( new entry_buffer (aSize));
    } else {
      if (theBuffers[aBufferName]->capacity() != aSize) {
        theBuffers[aBufferName]->resize(aSize);
      }
    }
  }

  void add(std::string const & aBufferName, Entry const & anEntry) {
    theBuffers[aBufferName]->push_back(anEntry);
  }

  void spill(std::string const & aBufferName, Format const & aFormat, std::ostream & anOstream) {
    entry_buffer::iterator iter(theBuffers[aBufferName]->begin());
    while (iter != theBuffers[aBufferName]->end()) {
      aFormat.format(anOstream, **iter);
      ++iter;
    }
  }
};

std::unique_ptr<SaveBufferManager> theSaveBufferManager;

inline SaveBufferManager & saveBufferManager() {
  if (theSaveBufferManager.get() == 0) {
    theSaveBufferManager.reset(new SaveBufferManager());
  }
  return *theSaveBufferManager;
}

SaveAction::SaveAction(std::string aBufferName, uint32_t aCircularBufferSize)
  : theBufferName(aBufferName)
  , theSize(aCircularBufferSize) {
  saveBufferManager().create(theBufferName, theSize);
}

void SaveAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "save (" << theBufferName << ") " << theSize << " ;";
}

void SaveAction::process(Entry const & anEntry) {
  saveBufferManager().add(theBufferName, anEntry);
}

FileSpillAction::FileSpillAction(std::string const & aBufferName, std::string const & aFilename, Format * aFormat)
  : theBufferName(aBufferName)
  , theFilename(aFilename)
  , theOstream(streamManager().getStream(aFilename) )
  , theFormat(aFormat)
{ };

void FileSpillAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "spill (" << theBufferName << ")->(" << theFilename << ") ";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void FileSpillAction::process(Entry const & anEntry) {
  saveBufferManager().spill(theBufferName, *theFormat, theOstream);
  theOstream.flush();
}

ConsoleSpillAction::ConsoleSpillAction(std::string const & aBufferName, Format * aFormat)
  : theBufferName(aBufferName)
  , theFormat(aFormat)
{ };

void ConsoleSpillAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "spill (" << theBufferName << ") -> console ";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void ConsoleSpillAction::process(Entry const & anEntry) {
  saveBufferManager().spill(theBufferName, *theFormat, std::cerr);
}
#endif

} // namespace Dbg
} // namespace Flexus
