/*=========================================================================

     Copyright (c) 2024 INRAE


     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef otbPrefetchCacheAsyncFilter_txx
#define otbPrefetchCacheAsyncFilter_txx

#include "otbPrefetchCacheAsyncFilter.h"

namespace otb
{

/**
 * Constructor.
 */
template <class TOutputImage>
PrefetchCacheAsyncFilter<TOutputImage>::PrefetchCacheAsyncFilter()
{
  // Set to zero the index and size of the cache image region
  m_CachedRegion.GetModifiableSize().Fill(0);
  m_CachedRegion.GetModifiableIndex().Fill(0);
  
  // Set to zero metrics accumulators
  m_MissedGuesses = 0;
  m_NbOfProcessedPixels = 0;
  m_ExtraGuesses = 0;
  m_GoodGuesses = 0;
}


/**
 * Constructor.
 */
template <class TOutputImage>
PrefetchCacheAsyncFilter<TOutputImage>::~PrefetchCacheAsyncFilter()
{
  // Wait for the thread to join
  if (m_Thread.joinable())
    m_Thread.join();

  // Compute performance metrics
  float percentMissed = 100 * m_MissedGuesses / m_NbOfProcessedPixels;
  float percentGood = 100 * m_GoodGuesses / m_NbOfProcessedPixels;
  float percentExtra = 100 * m_ExtraGuesses / m_NbOfProcessedPixels;
  
  // Display metris (todo: remove that and print it from the application)
  otbWarningMacro(<< m_MissedGuesses << " missing guessed pixels (" << percentMissed << " %)");
  otbWarningMacro(<< m_GoodGuesses << " good guessed pixels (" << percentGood << " %)");
  otbWarningMacro(<< m_ExtraGuesses << " extra guessed pixels (" << percentExtra << " %)");
}


/**
 * Generate the output image information (size, number of channels, etc).
 */
template <class TOutputImage>
void
PrefetchCacheAsyncFilter<TOutputImage>::GenerateOutputInformation(void)
{

  typename TOutputImage::Pointer outputPtr = this->GetOutput();
  unsigned int nBands;
  {
    std::lock_guard<std::mutex> guard(m_Mutex);
    nBands = GetInput()->GetNumberOfComponentsPerPixel();
  }
  outputPtr->SetNumberOfComponentsPerPixel(nBands);
  outputPtr->SetLargestPossibleRegion(GetInput()->GetLargestPossibleRegion());
  outputPtr->SetOrigin(GetInput()->GetOrigin());
  outputPtr->SetSignedSpacing(GetInput()->GetSignedSpacing());
  outputPtr->SetMetaDataDictionary(GetInput()->GetMetaDataDictionary());

}


/**
 * Retrieve a portion of the input image.
 * Modifies the buffer argument.
 */
template <class TOutputImage>
void
PrefetchCacheAsyncFilter<TOutputImage>::GetImageRegion(RegionType & region, typename TOutputImage::Pointer & buffer)
{
  std::lock_guard<std::mutex> guard(m_Mutex);

  otbDebugMacro(<< "Entering GetImageRegion() for region start " << region.GetIndex() << " size " << region.GetSize());
  
  // Trigger upstream pipeline
  otbDebugMacro(<< "Trigger upstream pipeline...");
  ImageType * inputImage = GetInput();
  inputImage->SetRequestedRegion(region);
  inputImage->PropagateRequestedRegion();
  inputImage->UpdateOutputData();
  otbDebugMacro(<< "Trigger upstream pipeline...done");
  
  unsigned int nBands = inputImage->GetNumberOfComponentsPerPixel();
  
  // Copy upstream pipeline result to buffer
  otbDebugMacro(<< "Copy upstream pipeline result to buffer");
  buffer = TOutputImage::New();
  buffer->SetBufferedRegion(region);
  buffer->SetNumberOfComponentsPerPixel(nBands);
  buffer->Allocate();

  ConstIteratorType inIt(inputImage, region);
  OutputIteratorType outIt(buffer, region);
  for (inIt.GoToBegin(); !inIt.IsAtEnd(); ++inIt, ++outIt)
  {
    outIt.Set(inIt.Get());
  }
  otbDebugMacro(<< "Exiting GetImageRegion()");

}


/**
 * Tell if a region is square
 */
template <class TOutputImage>
bool
PrefetchCacheAsyncFilter<TOutputImage>::IsSquare(RegionType & region)
{
  return region.GetSize(0) == region.GetSize(1);
}


/**
 * Guess the next input image region.
 * Modifies m_CachedRegion
 */
template <class TOutputImage>
bool
PrefetchCacheAsyncFilter<TOutputImage>::GuessNextRegion(RegionType & generatedRegion)
{

  otbDebugMacro(<< "Entering GuessNextRegion() for region start " << generatedRegion.GetIndex() << " size " << generatedRegion.GetSize());

  // We assume that the next region will have the same size
  SizeType size = generatedRegion.GetSize();

  // Compute shift with the previous region
  auto shift = generatedRegion.GetIndex() - m_PreviousGeneratedRegion.GetIndex();
  otbDebugMacro(<< "Raw shift: " << shift);
  
  auto preStart = m_PreviousGeneratedRegion.GetIndex();
  otbDebugMacro( "Previous start: " << preStart);
  auto curStart = generatedRegion.GetIndex();
  otbDebugMacro( "Current start: " << curStart);

  // When the shift is negative in x, we assume that it's a new line of tiles
  // thus we use the y value of the shift
  int shiftValue = (shift[0] <= 0) ? shift[1] : shift[0];
  otbDebugMacro(<< "Absolute shift value: " << shiftValue);
  
  // We assume that the next region will follow the same shift
  // in a specific dimension (i.e. row or col)
  unsigned int shiftDim = (preStart[0] == curStart[0]) ? 1 : 0;
  otbDebugMacro(<< "Shift along dimension " << shiftDim);
  auto start(curStart);
  start[shiftDim] += shiftValue;
  otbDebugMacro(<< "Guessed start: " << start);

  // If the older region is square, but not the one after,
  // we probably need to skip a row. So we recompute the region
  // as it will start on a new row
  bool preSquare = IsSquare(m_PreviousGeneratedRegion);
  otbDebugMacro(<< "Previous is square: " << preSquare);
  bool curSquare = IsSquare(generatedRegion);
  otbDebugMacro(<< "Current is square: " << curSquare);
  if (preSquare and !curSquare)
  {
    start[0] = 0;
    start[1] = generatedRegion.GetIndex(1) + shift[0]; // transpose shift
    size = m_PreviousGeneratedRegion.GetSize();
    otbDebugMacro(<< "Previous region was square, but not the last. Next might be a square region on a new row. New guessed start: " << start << " size: " << size);
  }
  m_CachedRegion = RegionType(start, size);

  std::lock_guard<std::mutex> guard(m_Mutex);
  return m_CachedRegion.Crop(GetInput()->GetLargestPossibleRegion());
  
}


/**
 * Retrieve the next input image region
 * m_CachedRegion, m_CachedBuffer are modified.
 */
template <class TOutputImage>
void
PrefetchCacheAsyncFilter<TOutputImage>::GetNextRegion(RegionType generatedRegion)
{
  otbDebugMacro(<< "Entering GetNextRegion() for region start " << generatedRegion.GetIndex() << " size " << generatedRegion.GetSize());

  // Guess the next region from the region that just has been generated
  bool guessed = GuessNextRegion(generatedRegion);

  // Cache the next region
  if (m_CachedRegion.GetNumberOfPixels() > 0)
  {
    if (guessed)
    {
      otbDebugMacro( << "Caching next guessed region (start " << m_CachedRegion.GetIndex() << " size " << m_CachedRegion.GetSize() << ") ...");
      GetImageRegion(m_CachedRegion, m_CachedBuffer);
      otbDebugMacro( << "Caching next guessed region...done");
    }
    else
    {
      // Reset the cached region so it is not used
      // when missing regions are computed
      otbDebugMacro( << "Not able to guess next region. reseting cached region to null.");
      m_CachedRegion.GetModifiableSize().Fill(0);
      m_CachedRegion.GetModifiableIndex().Fill(0);
    }
  }

  // At the end, update the previously generated region
  m_PreviousGeneratedRegion = RegionType(generatedRegion);
}

/**
 * Retrieve the regions that are missing from the cached image region.
 */
template <class TOutputImage>
typename PrefetchCacheAsyncFilter<TOutputImage>::RegionList
PrefetchCacheAsyncFilter<TOutputImage>::GetMissingRegions(RegionType & outputReqRegion)
{
  otbDebugMacro(
    << "Entering GetMissingRegions() for region start " << outputReqRegion.GetIndex() << " size " << outputReqRegion.GetSize()
    << " and the m_CachedRegion start " << m_CachedRegion.GetIndex() << " size " << m_CachedRegion.GetSize()
  );
  
  // This is for statistics and also to check that the cached region touches
  // the requested one.
  RegionType forStats(m_CachedRegion);
  m_NbOfProcessedPixels += outputReqRegion.GetNumberOfPixels();
  float nbOfCachedPixels = forStats.GetNumberOfPixels();
  bool touches = forStats.Crop(outputReqRegion);
  if (touches)
  {
    m_ExtraGuesses += nbOfCachedPixels - forStats.GetNumberOfPixels();
    m_GoodGuesses += forStats.GetNumberOfPixels();
    m_MissedGuesses += outputReqRegion.GetNumberOfPixels() - forStats.GetNumberOfPixels();
  }
  else
  {
    m_ExtraGuesses += nbOfCachedPixels;
    m_MissedGuesses += outputReqRegion.GetNumberOfPixels();
  }
  
  if (m_CachedRegion.IsInside(outputReqRegion))
    // When the output requested region lies entirely inside the
    // cached region, there is nothing missing :)
    return RegionList({});
  
  if (m_CachedRegion.GetNumberOfPixels() == 0 || !touches)
    // When the cached region is empty, it means that there is a single
    // missing region which is the entire requested region.
    return RegionList({outputReqRegion});
  
  // Else, we collect the missing regions.
  IndexType outStart = outputReqRegion.GetIndex();
  IndexType outEnd = outputReqRegion.GetUpperIndex();
  IndexType cacheStart = m_CachedRegion.GetIndex();
  IndexType cacheEnd = m_CachedRegion.GetUpperIndex();
  
  RegionList regions;
  std::vector<std::vector<RegionSlice1D>> dim_splits;
  for (unsigned int dim = 0; dim < ImageType::ImageDimension; ++dim)
  {
    auto rngStart = std::max(outStart[dim], cacheStart[dim]);
    auto rngEnd   = std::min(outEnd[dim], cacheEnd[dim]);
    std::vector<RegionSlice1D> splits; 
    if (outStart[dim] <= rngStart and rngEnd <= outEnd[dim])
      splits.push_back(RegionSlice1D(rngStart, rngEnd - rngStart + 1, true));
    if (outStart[dim] < rngStart)
      splits.push_back(RegionSlice1D(outStart[dim], rngStart - outStart[dim], false));
    if (rngEnd < outEnd[dim])
      splits.push_back(RegionSlice1D(rngEnd + 1, outEnd[dim] - rngEnd, false));
    dim_splits.push_back(splits);
  }
  
  // Todo: nested loop for generic number of dimensions 
  // (here only dimension 2 is deals with)
  for (auto& x_split: dim_splits[0])
    for (auto& y_split: dim_splits[1])
      if (!x_split.cached || !y_split.cached)
        regions.push_back(RegionType(IndexType({x_split.start, y_split.start}), SizeType({x_split.size, y_split.size})));
  
  return regions;
}



/**
 * Compute the output image on the requested region.
 */
template <class TOutputImage>
void
PrefetchCacheAsyncFilter<TOutputImage>::GenerateData()
{
  otbDebugMacro(<< "\033[1;31m\nEntering GenerateData()\033[0m\n");

  otbDebugMacro(<< "Waiting thread...");
  if (m_Thread.joinable())
  {
    otbDebugMacro(<< "Thread is running");
    m_Thread.join();
  }
  otbDebugMacro(<< "Waiting thread...done");


  // Output pointer and requested region
  typename TOutputImage::Pointer outputPtr = this->GetOutput();
  RegionType                     outputReqRegion = outputPtr->GetRequestedRegion();
  otbDebugMacro(<< "Requested region start " << outputReqRegion.GetIndex() << " size " << outputReqRegion.GetSize());
    
  // Find the missing parts i.e. regions outside the cached region
  RegionList missingRegions = GetMissingRegions(outputReqRegion);
  std::vector<typename TOutputImage::Pointer> missingBuffers;
  for (auto region: missingRegions)
  {
    otbDebugMacro(<< "Missing region start " << region.GetIndex() << " size " << region.GetSize());
    typename TOutputImage::Pointer buffer;
    GetImageRegion(region, buffer);
    missingBuffers.push_back(buffer);
  }
  if (missingRegions.size() == 0)
    otbDebugMacro(<< "No missing region");

  
  // Prepare the output buffer
  otbDebugMacro(<< "Prepare the output buffer");
  unsigned int nBands;
  {
    std::lock_guard<std::mutex> guard(m_Mutex);
    nBands = GetInput()->GetNumberOfComponentsPerPixel();
  }
  outputPtr->SetBufferedRegion(outputReqRegion);
  outputPtr->SetNumberOfComponentsPerPixel(nBands);
  outputPtr->Allocate();
  
  // Fill
  otbDebugMacro(<< "Fill");
  RegionList allRegions(missingRegions);
  std::vector<typename TOutputImage::Pointer> allBuffers(missingBuffers);

  RegionType cachedRegionForOutput(m_CachedRegion);
  cachedRegionForOutput.Crop(outputReqRegion);
  if (cachedRegionForOutput.GetNumberOfPixels() > 0)
  {
    allRegions.push_back(cachedRegionForOutput);
    allBuffers.push_back(m_CachedBuffer);
  }
  std::vector<ConstIteratorType> allIterators;
  otbDebugMacro(<< "Prepare allIterators...");
  for (unsigned int i = 0; i < allBuffers.size(); i++)
  {
    otbDebugMacro(
      << "Region " << i << " start " << allRegions[i].GetIndex() << " size " << allRegions[i].GetSize() 
      << " (number of pixels: " << allRegions[i].GetNumberOfPixels() << ")" << std::endl
    );
    ConstIteratorType it(allBuffers[i], allRegions[i]);
    allIterators.push_back(it);
    otbDebugMacro(<< "New iterator created");
  }
  OutputIteratorType outIt(outputPtr, outputReqRegion);
  for (outIt.GoToBegin(); !outIt.IsAtEnd(); ++outIt)
  {
    for (unsigned int i = 0; i < allBuffers.size(); i++)
    {
      if (allRegions[i].IsInside(outIt.GetIndex()))
      {
        // We can do that since iterators only stay a region
        // that results from the buffer region clipped with
        // the outputReqRegion.
        // When the region is null, the corresponding buffer is
        // not used.
        outIt.Set(allIterators[i].Get());
        ++allIterators[i];
      }
    }
  }
  otbDebugMacro(<< "Fill complete");
  

  // Fire and forget
  otbDebugMacro(<< "Fire and forget");
  m_Thread = std::thread(&PrefetchCacheAsyncFilter<TOutputImage>::GetNextRegion, this, outputReqRegion);
  
}


} // end namespace otb


#endif
