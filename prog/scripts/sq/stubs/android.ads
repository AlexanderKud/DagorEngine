
return {
  addProviderInit = @(_provider, _id) false
  addProviderInitWithPriority = @(_, __, ___) null
  isAdsInited = @() false
  isAdsInitedForProvider = @(_) false
  setPriorityForProvider = @(_, __) null
  loadAds = function() {}
  showAds = function() {}
  isAdsLoaded = @() false
  requestConsent = @(_) null
  showPrivacy = @() null
  showConsent = @() null
  getConsentStatus = @() 0
  canRequestAds = @() false
  setConsent = @(_) null
  setMetaData = @(_, __) null
  getProvidersStatus = @() ""
  ADS_STATUS_REWARDED = 0
  ADS_STATUS_NOT_INITED = 1
  ADS_STATUS_NOT_READY = 2
  ADS_STATUS_FAIL = 3
  ADS_STATUS_DISMISS = 4
  ADS_STATUS_SHOWN = 5
  ADS_STATUS_LOADED = 6
  ADS_STATUS_NOT_FOUND = 7
  ADS_STATUS_OK = 10
  ADS_STATUS_FAIL_IN_QUEUE_SKIP = 11
  CONSENT_REQUEST_NOT_REQUIRED = 1
  CONSENT_REQUEST_OBTAINED = 3
  CONSENT_REQUEST_REQUIRED = 2
  CONSENT_REQUEST_UNKNOWN = 0
}