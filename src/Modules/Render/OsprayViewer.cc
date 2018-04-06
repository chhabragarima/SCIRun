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

#include <es-log/trace-log.h>
#include <Modules/Render/OsprayViewer.h>
#include <Modules/Render/ViewScene.h>
#include <Core/Logging/Log.h>


using namespace SCIRun::Modules::Render;
using namespace SCIRun::Core::Algorithms;
using namespace SCIRun::Core::Datatypes;
using namespace SCIRun::Dataflow::Networks;
using namespace SCIRun::Core::Thread;
using namespace SCIRun::Core::Logging;
using namespace SCIRun::Core::Algorithms::Render;

MODULE_INFO_DEF(OsprayViewer, Render, SCIRun)

OsprayViewer::OsprayViewer() : ModuleWithAsyncDynamicPorts(staticInfo_, true)
{
  RENDERER_LOG_FUNCTION_SCOPE;
  INITIALIZE_PORT(GeneralGeom);
}

void OsprayViewer::setStateDefaults()
{
  auto state = get_state();

}

void OsprayViewer::portRemovedSlotImpl(const PortId& pid)
{

}

void OsprayViewer::asyncExecute(const PortId& pid, DatatypeHandle data)
{
  auto geom = boost::dynamic_pointer_cast<CompositeOsprayGeometryObject>(data);
  if (!geom)
  {
    error("Logical error: not a geometry object on OsprayViewer");
    return;
  }

  get_state()->setTransientValue(Parameters::GeomData, geom->objects()[0], true);
}


void OsprayViewer::execute()
{
}
