/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2015 Scientific Computing and Imaging Institute,
   University of Utah.

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

#include <fstream>
#include <boost/filesystem.hpp>
#include <Core/Algorithms/Base/AlgorithmPreconditions.h>
#include <Core/Algorithms/DataIO/WriteBundle.h>
#include <Core/Algorithms/Base/AlgorithmVariableNames.h>
#include <Core/Datatypes/DenseMatrix.h>
//#include <Core/Datatypes/MatrixIO.h>

using namespace SCIRun::Core::Algorithms;
using namespace SCIRun::Core::Algorithms::DataIO;
using namespace SCIRun::Core::Datatypes;

WriteBundleAlgorithm::WriteBundleAlgorithm()
{
  addParameter(Variables::Filename, std::string(""));
}

WriteBundleAlgorithm::Outputs WriteBundleAlgorithm::run(const WriteBundleAlgorithm::Inputs& inputBundle, const WriteBundleAlgorithm::Parameters& filename) const
{
  ENSURE_ALGORITHM_INPUT_NOT_NULL(inputBundle, "Cannot write null bundle.");

  if (boost::filesystem::extension(filename) == ".txt")
  {
    std::ofstream writer(filename.c_str());
    writer << *inputBundle;
  }
  else if (boost::filesystem::extension(filename) == ".bdl")
  {
    status("Writing bundle file as binary .bdl");

    // Open up the output stream
    PiostreamPtr stream = auto_ostream(filename, "Binary");
    
    if (stream->error()) 
    {
      error("Could not open file for writing: " + filename);
    } 
    else 
    {
      //BOOO const_cast
      Pio(*stream, const_cast<WriteBundleAlgorithm::Inputs&>(inputBundle));
    } 
  }
  if (!boost::filesystem::exists(filename))
    THROW_ALGORITHM_PROCESSING_ERROR("file failed to be written!");
}

AlgorithmOutput WriteBundleAlgorithm::run(const AlgorithmInput& input) const
{
  auto filename = get(Variables::Filename).toFilename().string();
  run(input.get<Bundle>(Variables::BundleToWrite), filename);
  return AlgorithmOutput();
}
