/**
 * e_return_error_if_fail:
 *
 * FIXME Document me!
 *
 * Since: 2.30
 **/
#define e_return_error_if_fail(expr,error_code)	G_STMT_START{		\
     if G_LIKELY(expr) { } else						\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__,						\
		__LINE__,						\
		G_STRFUNC,						\
		#expr);							\
	 g_set_error (error, E_BOOK_ERROR, (error_code),		\
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__,						\
		__LINE__,						\
		G_STRFUNC,						\
		#expr);							\
	 return FALSE;							\
       };				}G_STMT_END

/**
 * e_return_async_error_if_fail:
 *
 * Since: 2.32
 **/
#define e_return_async_error_if_fail(expr, error) G_STMT_START { \
    if G_LIKELY (expr) {} else {                                 \
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,                 \
             "file %s: line %d (%s): assertion `%s' failed",     \
             __FILE__, __LINE__, G_STRFUNC, #expr);    \
      cb (book, error, closure);                           \
      return 0;                                                  \
    }                                                            \
  } G_STMT_END                                                   \

#define e_return_async_error_val_if_fail(expr, error) G_STMT_START { \
    if G_LIKELY (expr) {} else {                                 \
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,                 \
             "file %s: line %d (%s): assertion `%s' failed",     \
             __FILE__, __LINE__, G_STRFUNC, #expr);    \
      cb (book, error, NULL, closure);                           \
      return 0;                                                  \
    }                                                            \
  } G_STMT_END                                                   \

#define e_return_ex_async_error_if_fail(expr, error) G_STMT_START {	\
    if G_LIKELY (expr) {} else {					\
      GError *err;							\
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,			\
             "file %s: line %d (%s): assertion `%s' failed",		\
             __FILE__, __LINE__, G_STRFUNC, #expr);			\
      err = g_error_new (E_BOOK_ERROR, (error),				\
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__, __LINE__, G_STRFUNC, #expr);			\
      cb (book, err, closure);					\
      g_error_free (err);						\
      return FALSE;							\
    }								\
  } G_STMT_END

/**
 * e_return_ex_async_error_val_if_fail:
 *
 * Since: 2.32
 **/
#define e_return_ex_async_error_val_if_fail(expr, error) G_STMT_START {	\
    if G_LIKELY (expr) {} else {					\
      GError *err;							\
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,			\
             "file %s: line %d (%s): assertion `%s' failed",		\
             __FILE__, __LINE__, G_STRFUNC, #expr);			\
      err = g_error_new (E_BOOK_ERROR, (error),				\
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__, __LINE__, G_STRFUNC, #expr);			\
      cb (book, err, NULL, closure);					\
      g_error_free (err);						\
      return 0;							\
    }								\
  } G_STMT_END
