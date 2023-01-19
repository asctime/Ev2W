


#include <e-data-cal.h>
#include "e-data-cal-enumtypes.h"

/* enumerations from "e-data-cal-types.h" */
GType
e_data_cal_call_status_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { Success, "Success", "Success" },
      { RepositoryOffline, "RepositoryOffline", "RepositoryOffline" },
      { PermissionDenied, "PermissionDenied", "PermissionDenied" },
      { InvalidRange, "InvalidRange", "InvalidRange" },
      { ObjectNotFound, "ObjectNotFound", "ObjectNotFound" },
      { InvalidObject, "InvalidObject", "InvalidObject" },
      { ObjectIdAlreadyExists, "ObjectIdAlreadyExists", "ObjectIdAlreadyExists" },
      { AuthenticationFailed, "AuthenticationFailed", "AuthenticationFailed" },
      { AuthenticationRequired, "AuthenticationRequired", "AuthenticationRequired" },
      { UnsupportedField, "UnsupportedField", "UnsupportedField" },
      { UnsupportedMethod, "UnsupportedMethod", "UnsupportedMethod" },
      { UnsupportedAuthenticationMethod, "UnsupportedAuthenticationMethod", "UnsupportedAuthenticationMethod" },
      { TLSNotAvailable, "TLSNotAvailable", "TLSNotAvailable" },
      { NoSuchCal, "NoSuchCal", "NoSuchCal" },
      { UnknownUser, "UnknownUser", "UnknownUser" },
      { OfflineUnavailable, "OfflineUnavailable", "OfflineUnavailable" },
      { SearchSizeLimitExceeded, "SearchSizeLimitExceeded", "SearchSizeLimitExceeded" },
      { SearchTimeLimitExceeded, "SearchTimeLimitExceeded", "SearchTimeLimitExceeded" },
      { InvalidQuery, "InvalidQuery", "InvalidQuery" },
      { QueryRefused, "QueryRefused", "QueryRefused" },
      { CouldNotCancel, "CouldNotCancel", "CouldNotCancel" },
      { OtherError, "OtherError", "OtherError" },
      { InvalidServerVersion, "InvalidServerVersion", "InvalidServerVersion" },
      { InvalidArg, "InvalidArg", "InvalidArg" },
      { NotSupported, "NotSupported", "NotSupported" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("EDataCalCallStatus", values);
  }
  return etype;
}
GType
e_data_cal_view_listener_set_mode_status_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ModeSet, "ModeSet", "ModeSet" },
      { ModeNotSet, "ModeNotSet", "ModeNotSet" },
      { ModeNotSupported, "ModeNotSupported", "ModeNotSupported" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("EDataCalViewListenerSetModeStatus", values);
  }
  return etype;
}
GType
e_data_cal_obj_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { Event, "Event", "Event" },
      { Todo, "Todo", "Todo" },
      { Journal, "Journal", "Journal" },
      { AnyType, "AnyType", "AnyType" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("EDataCalObjType", values);
  }
  return etype;
}
GType
e_data_cal_obj_mod_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { This, "This", "This" },
      { ThisAndPrior, "ThisAndPrior", "ThisAndPrior" },
      { ThisAndFuture, "ThisAndFuture", "ThisAndFuture" },
      { All, "All", "All" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("EDataCalObjModType", values);
  }
  return etype;
}
GType
e_data_cal_mode_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { Local, "Local", "Local" },
      { Remote, "Remote", "Remote" },
      { AnyMode, "AnyMode", "AnyMode" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("EDataCalMode", values);
  }
  return etype;
}



