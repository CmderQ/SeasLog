/*
+----------------------------------------------------------------------+
| SeasLog                                                              |
+----------------------------------------------------------------------+
| This source file is subject to version 2.0 of the Apache license,    |
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.apache.org/licenses/LICENSE-2.0.html                      |
| If you did not receive a copy of the Apache2.0 license and are unable|
| to obtain it through the world-wide-web, please send a note to       |
| license@php.net so we can mail you a copy immediately.               |
+----------------------------------------------------------------------+
| Author: Neeke.Gao  <neeke@php.net>                                   |
+----------------------------------------------------------------------+
*/

static php_stream *seaslog_stream_open_wrapper(char *opt TSRMLS_DC)
{
    php_stream *stream = NULL;
    char *res = NULL;
    int first_create_file = 0;
    mode_t file_mode;

#if PHP_VERSION_ID >= 70000
    zend_long reslen;
#else
    long reslen;
#endif

    switch SEASLOG_G(appender)
    {
    case SEASLOG_APPENDER_TCP:
        reslen = spprintf(&res, 0, "tcp://%s:%d", SEASLOG_G(remote_host), SEASLOG_G(remote_port));
        stream = php_stream_xport_create(res, reslen, REPORT_ERRORS, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT, 0, 0, NULL, NULL, NULL);
        efree(res);
        break;

    case SEASLOG_APPENDER_UDP:
        reslen = spprintf(&res, 0, "udp://%s:%d", SEASLOG_G(remote_host), SEASLOG_G(remote_port));
        stream = php_stream_xport_create(res, reslen, REPORT_ERRORS, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT, 0, 0, NULL, NULL, NULL);

        efree(res);
        break;
    case SEASLOG_APPENDER_FILE:
    default:
        if (access(opt,F_OK) != 0)
        {
            first_create_file = 1;
        }

        stream = php_stream_open_wrapper(opt, "a", IGNORE_URL_WIN | REPORT_ERRORS, NULL);

        if (first_create_file == 1)
        {
            file_mode = (mode_t) SEASLOG_FILE_MODE;
            VCWD_CHMOD(opt, file_mode);
        }
    }

    return stream;
}

static int seaslog_init_stream_list(TSRMLS_D)
{
    zval *z_stream_list;

#if PHP_VERSION_ID >= 70000
    array_init(&SEASLOG_G(stream_list));
#else
    SEASLOG_G(stream_list) = NULL;

    MAKE_STD_ZVAL(z_stream_list);
    array_init(z_stream_list);

    SEASLOG_G(stream_list) = z_stream_list;
#endif

}

static int seaslog_clear_stream_list(TSRMLS_D)
{
    php_stream *stream = NULL;
    HashTable *ht;

#if PHP_VERSION_ID >= 70000
    zend_ulong num_key;
    zend_string *str_key;
    zval *stream_zval_get_php7;
#else
    zval **stream_zval_get;
#endif

#if PHP_VERSION_ID >= 70000
    if (Z_TYPE(SEASLOG_G(stream_list)) == IS_ARRAY)
    {
        ht = Z_ARRVAL(SEASLOG_G(stream_list));
        ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, stream_zval_get_php7)
        {
            php_stream_from_zval_no_verify(stream,stream_zval_get_php7);
            if (stream)
            {
                php_stream_close(stream);
                php_stream_free(stream, PHP_STREAM_FREE_RELEASE_STREAM);
            }
        }
        ZEND_HASH_FOREACH_END();

        EX_ARRAY_DESTROY(&SEASLOG_G(stream_list));
    }
#else
    if (SEASLOG_G(stream_list) && Z_TYPE_P(SEASLOG_G(stream_list)) == IS_ARRAY)
    {
        ht = HASH_OF(SEASLOG_G(stream_list));

        zend_hash_internal_pointer_reset(ht);
        while (zend_hash_get_current_data(ht, (void **)&stream_zval_get) == SUCCESS)
        {
            php_stream_from_zval_no_verify(stream,stream_zval_get);
            if (stream)
            {
                php_stream_close(stream);
                php_stream_free(stream, PHP_STREAM_FREE_RELEASE_STREAM);
            }
            zend_hash_move_forward(ht);
        }

        EX_ARRAY_DESTROY(&(SEASLOG_G(stream_list)));
    }
#endif

}

static php_stream *process_stream(char *opt, int opt_len TSRMLS_DC)
{
    ulong stream_entry_hash;
    php_stream *stream = NULL;
    zval *stream_zval;
    zval **stream_zval_get;
    HashTable *ht_list;
#if PHP_VERSION_ID >= 70000
    zval stream_zval_to;
#else
    zval *stream_zval_to;
#endif

    switch SEASLOG_G(appender)
    {
    case SEASLOG_APPENDER_TCP:
        stream_entry_hash = zend_inline_hash_func("tcp", 4);
        break;
    case SEASLOG_APPENDER_UDP:
        stream_entry_hash = zend_inline_hash_func("udp", 4);
        break;
    case SEASLOG_APPENDER_FILE:
    default:
        stream_entry_hash = zend_inline_hash_func(opt, opt_len);
    }

#if PHP_VERSION_ID >= 70000
    ht_list = Z_ARRVAL(SEASLOG_G(stream_list));

    if ((stream_zval = zend_hash_index_find(ht_list, stream_entry_hash)) != NULL)
    {
        php_stream_from_zval_no_verify(stream,stream_zval);

        return stream;
#else
    ht_list = HASH_OF(SEASLOG_G(stream_list));

    if (zend_hash_index_find(ht_list, stream_entry_hash, (void **)&stream_zval_get) == SUCCESS)
    {
        php_stream_from_zval_no_verify(stream,stream_zval_get);

        return stream;
#endif
    }
    else
    {
        stream = seaslog_stream_open_wrapper(opt TSRMLS_CC);

#if PHP_VERSION_ID >= 70000
        php_stream_to_zval(stream, &stream_zval_to);
#else
        MAKE_STD_ZVAL(stream_zval_to);
        php_stream_to_zval(stream, stream_zval_to);
#endif
        SEASLOG_ADD_INDEX_ZVAL(SEASLOG_G(stream_list),stream_entry_hash,stream_zval_to);
        return stream;
    }

    return stream;
}