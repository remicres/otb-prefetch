set(DOCUMENTATION "This module provide a filter and an application to prefetch the input in an asynchronous fashion.")

otb_module(OTBPrefetch
  DEPENDS
    OTBCommon
    OTBApplicationEngine

  TEST_DEPENDS

  DESCRIPTION
    "${DOCUMENTATION}"
  COMPONENT
    Core
)

