/*=========================================================================

     Copyright (c) 2024 INRAE


     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef otbPrefetchCacheAsyncFilter_h
#define otbPrefetchCacheAsyncFilter_h

#include "itkImageSource.h"

// Iterator
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"

// OTB log
#include "otbMacro.h"
#include "itkMacro.h"

#include <thread>
#include <vector>
#include <chrono>

namespace otb
{

/**
 * \class PrefetchCacheAsyncFilter
 * \brief This filter prefetches the input image, in an asynchronous fashion.
 *
 * The filter takes one input image and copies it to the output. This filters 
 * has a thread that prefetches the input image while the downstream filter is 
 * running. To do that, it tries to guess what is the next requested region, 
 * based on the previous one, and the assumption that the next requested region 
 * mostly differs from the previous one, by an offset which is frequently the 
 * same. When the filter fails to guess the right next requested region, this
 * is not so bad because it will grab the missing parts during the call to 
 *`GenerateData()` (hence, in a synchronous fashion) before generating the 
 * output image.
 *
 * \ingroup ...
 */
template <class TOutputImage>
class ITK_EXPORT PrefetchCacheAsyncFilter : public itk::ImageSource<TOutputImage>
{

public:
  /** Standard class typedefs. */
  typedef PrefetchCacheAsyncFilter               Self;
  typedef itk::ImageSource<TOutputImage> Superclass;
  typedef itk::SmartPointer<Self>        Pointer;
  typedef itk::SmartPointer<const Self>  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(PrefetchCacheAsyncFilter, ImageSource);

  /** Images typedefs */
  typedef TOutputImage                     ImageType;
  typedef typename ImageType::IndexType    IndexType;
  typedef typename ImageType::SizeType     SizeType;
  typedef typename ImageType::RegionType   RegionType;
  typedef typename std::vector<RegionType> RegionList;

  /* Iterators typedefs */
  typedef typename itk::ImageRegionIterator<TOutputImage> OutputIteratorType;
  typedef typename itk::ImageRegionConstIterator<TOutputImage> ConstIteratorType;
  
  itkGetMacro(ExtraGuesses, float);
  itkGetMacro(MissedGuesses, float);
  itkGetMacro(GoodGuesses, float);
  itkGetMacro(NbOfProcessedPixels, float);
  
  /* Region slice 1D */
  struct RegionSlice1D {
    RegionSlice1D(int strt, int sz, bool cch): start(strt), size(sz), cached(cch) {};
    int start;
    long unsigned int size;
    bool cached;
  };
  
  void SetInput(ImageType * input)
  {
    m_InputImage = input;
  }
  
  ImageType * GetInput()
  {
    return m_InputImage;
  }


protected:
  PrefetchCacheAsyncFilter();
  ~PrefetchCacheAsyncFilter();
  
  void GenerateOutputInformation(void);
  
  void GetImageRegion(RegionType & region, typename TOutputImage::Pointer & buffer);
  
  bool IsSquare(RegionType & region);
  
  bool GuessNextRegion(RegionType & generatedRegion);
  
  void GetNextRegion(RegionType generatedRegion);
  
  RegionList GetMissingRegions(RegionType & outputReqRegion);

  void GenerateData();

private:
  PrefetchCacheAsyncFilter(const Self &); // purposely not implemented
  void operator=(const Self &); // purposely not implemented
  
  ImageType * m_InputImage;
  std::thread m_Thread;
  RegionType m_PreviousGeneratedRegion;
  RegionType m_CachedRegion;
  typename TOutputImage::Pointer m_CachedBuffer;
  float m_ExtraGuesses;
  float m_MissedGuesses;
  float m_GoodGuesses;
  float m_NbOfProcessedPixels;
  float m_WaitSecs;

  
}; // end class


} // end namespace otb

#include "otbPrefetchCacheAsyncFilter.hxx"

#endif
