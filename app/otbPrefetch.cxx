/*=========================================================================

     Copyright (c) 2024 INRAE


     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "otbWrapperApplication.h"
#include "otbWrapperApplicationFactory.h"

#include "otbPrefetchCacheAsyncFilter.h"

namespace otb
{
namespace Wrapper
{

class Prefetch : public Application
{
public:
  using Self = Prefetch;
  using Superclass = Application;
  using Pointer = itk::SmartPointer<Self>;
  using ConstPointer = itk::SmartPointer<const Self>;
  itkNewMacro(Self);
  itkTypeMacro(Prefetch, otb::Application);
  using FilterType = otb::PrefetchCacheAsyncFilter<FloatVectorImageType>;

private:
  void DoInit() override
  {
    SetName("Prefetch");
    SetDescription(
      "This application prefetches its input in an asynchronous fashion, "
      "letting the downstream pipeline running while trying to guess and "
      "cache the next requested region."
    );

    SetDocLimitations(
      "The next output streaming region is guessed from the previous one. "
      "It is mostly optimized for tiled and stripped splits. Hence when downstream "
      "filters do otherwise, it can fail to optimize upstream calls."
    );

    SetDocAuthors("Remi Cresson");
    AddParameter(ParameterType_InputImage, "in", "Input image");
    AddParameter(ParameterType_OutputImage, "out", "Output image");
  }
  
  
  void DoUpdateParameters() override {} // nothing to do here (parameters are independant).

  void DoExecute() override
  {
    auto filter = FilterType::New();
    filter->SetInput(GetParameterImage("in"));
    SetParameterOutputImage("out", filter->GetOutput());
    RegisterPipeline();
  }
  
  /**
  TODO when https://gitlab.orfeo-toolbox.org/orfeotoolbox/otb/-/issues/2374 is fixed
  void AfterExecuteAndWriteOutputs() override
  {

    float nbOfProcessedPixels = filter->GetNbOfProcessedPixels();
    float percentMissed = 100 * filter->GetMissedGuesses() / nbOfProcessedPixels;
    float percentGood = 100 * filter->GetGoodGuesses() / nbOfProcessedPixels;
    float percentExtra = 100 * filter->GetExtraGuesses() / nbOfProcessedPixels;
    otbAppLogINFO(<< filter->GetMissedGuesses() << " missing guessed pixels (" << percentMissed << " %)");
    otbAppLogINFO(<< filter->GetGoodGuesses() << " good guessed pixels (" << percentGood << " %)");
    otbAppLogINFO(<< filter->GetExtraGuesses() << " extra guessed pixels (" << percentExtra << " %)");

  }
  */
};

}
}

OTB_APPLICATION_EXPORT(otb::Wrapper::Prefetch)

