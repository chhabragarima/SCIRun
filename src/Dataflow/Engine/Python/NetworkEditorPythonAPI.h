/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2015 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/// @todo Documentation Dataflow/Engine/Python/NetworkEditorPythonAPI.cc

#ifndef ENGINE_PYTHON_NETWORKEDITORPYTHONAPI_H
#define ENGINE_PYTHON_NETWORKEDITORPYTHONAPI_H

#include <vector>
#include <Dataflow/Network/NetworkFwd.h>
#include <Dataflow/Engine/Python/share.h>

namespace SCIRun {

  class NetworkEditorPythonInterface;
  class PyModule;

  class SCISHARE NetworkEditorPythonAPI
  {
  public:
    static boost::shared_ptr<PyModule> addModule(const std::string& name);
    static std::vector<boost::shared_ptr<PyModule>> modules();
    static std::string removeModule(const std::string& id);
    static std::string connect(const std::string& moduleIdFrom, int fromIndex, const std::string& moduleIdTo, int toIndex);
    static std::string disconnect(const std::string& moduleIdFrom, int fromIndex, const std::string& moduleIdTo, int toIndex);

    static std::string executeAll();
    static std::string saveNetwork(const std::string& filename);
    static std::string loadNetwork(const std::string& filename);
    static std::string quit(bool force);

    static void setImpl(boost::shared_ptr<NetworkEditorPythonInterface> impl);
    /// @todo: smelly!
    static void setExecutionContext(Dataflow::Networks::ExecutableLookup* lookup);
  private:
    NetworkEditorPythonAPI();
    static boost::shared_ptr<NetworkEditorPythonInterface> impl_;
    static Dataflow::Networks::ExecutableLookup* lookup_;
    static std::vector<boost::shared_ptr<PyModule>> modules_;
  };

  class SCISHARE SimplePythonAPI
  {
  public:
    static std::string scirun_add_module(const std::string& name);
    static std::string scirun_connect_modules(const std::string& modIdFrom, int fromIndex, const std::string& modIdTo, int toIndex);
    static std::string scirun_disconnect_modules(const std::string& modIdFrom, int fromIndex, const std::string& modIdTo, int toIndex);
    static std::string scirun_quit(bool force);
  private:
    SimplePythonAPI();
  };

}

#endif
