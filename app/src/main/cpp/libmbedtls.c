//
// Created by Siddarth Gandhi on 2019-05-10.
//

#include <jni.h>
#include <string.h>
#include <assert.h>
#include "libmbedtls.h"
#include "mbedtls/debug.h"

jobject classReference;
jmethodID writeCallback;
jmethodID readCallback;
jmethodID debugUtility;

static JavaVM *jvm;

jintArray ciphers;

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_init(JNIEnv *env, jobject thisObj) {
    mbedtls_ssl_init(&ssl_context);
    mbedtls_ssl_config_init(&ssl_config);
    mbedtls_ctr_drbg_init(&random_byte_generator);
    mbedtls_entropy_init(&entropy_context);
    mbedtls_x509_crt_init(&cert_chain1);
    mbedtls_x509_crt_init(&cert_chain2);
    mbedtls_x509_crt_init(&cert_chain3);
    mbedtls_pk_init(&key_pair);

    mbedtls_ctr_drbg_seed(&random_byte_generator, mbedtls_entropy_func, &entropy_context, NULL, 0);

    //Cache JavaVM pointer
    (*env)->GetJavaVM(env, &jvm);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_setupSSLContext(JNIEnv *env, jobject thisObj) {
    mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_rng(&ssl_config, mbedtls_ctr_drbg_random, &random_byte_generator);
    mbedtls_ssl_setup(&ssl_context, &ssl_config);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_getClassObject(JNIEnv *env, jobject thisObj, jobject classref) {
    classReference = (*env)->NewGlobalRef(env, classref);
    jclass mbedTLS = (*env)->GetObjectClass(env, classref);
    writeCallback = (*env)->GetMethodID(env, mbedTLS, "writeCallback", "([BI)I");
    readCallback = (*env)->GetMethodID(env, mbedTLS, "readCallback", "(I)[B");
    debugUtility = (*env)->GetMethodID(env, mbedTLS, "debugUtility", "([BI[B)V");
}

int write_callback(void *ctx, const unsigned char *buf, size_t len) {
    JNIEnv *env;
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    jbyteArray arr = (*env)->NewByteArray(env, (jsize)len);
    (*env)->SetByteArrayRegion(env, arr, 0, (jsize)len, (jbyte*)buf);
    jint result = (*env)->CallIntMethod(env, classReference, writeCallback, arr, len);
    return result;
}

int read_callback(void *ctx, unsigned char *buf, size_t len) {
    JNIEnv *env;
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    jbyteArray bytesToRead = (*env)->CallObjectMethod(env, classReference, readCallback, len);
    (*env)->GetByteArrayRegion(env, bytesToRead, 0, (jsize)len, (jbyte*)buf);
    return (int)len;
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_setIOFuncs(JNIEnv *env, jobject thisObj, jstring contextParameter) {
    mbedtls_ssl_set_bio(&ssl_context, &contextParameter, write_callback, read_callback, NULL);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_configureCipherSuites(JNIEnv *env, jobject thisObj, jintArray ciphersuites) {
    ciphers = (*env)->GetIntArrayElements(env, ciphersuites, 0);
    mbedtls_ssl_conf_ciphersuites(&ssl_config, ciphers);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_setMinimumProtocolVersion(JNIEnv *env, jobject thisObj, jint version) {
    mbedtls_ssl_conf_min_version(&ssl_config, MBEDTLS_SSL_MAJOR_VERSION_3, version);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_setMaximumProtocolVersion(JNIEnv *env, jobject thisObj, jint version) {
    mbedtls_ssl_conf_max_version(&ssl_config, MBEDTLS_SSL_MAJOR_VERSION_3, version);
}

JNIEXPORT jint JNICALL Java_com_simplisafe_mbedtls_mbedTLS_executeHandshakeStep(JNIEnv *env, jobject thisObj) {
    return mbedtls_ssl_handshake_client_step(&ssl_context);
}

int get_array_size(const char *arr) {
    int size = 0;
    while (arr[size] != '\0') size++;
    return size;
}

void debug_msg(void *ctx, int level, const char *file, int line, const char *str) {
    JNIEnv *env;
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    jbyteArray fileName = (*env)->NewByteArray(env, get_array_size(file));
    (*env)->SetByteArrayRegion(env, fileName, 0, (jsize)get_array_size(file), (jbyte*)file);
    jbyteArray log = (*env)->NewByteArray(env, get_array_size(str));
    (*env)->SetByteArrayRegion(env, log, 0, (jsize)get_array_size(str), (jbyte*)str);
    (*env)->CallVoidMethod(env, classReference, debugUtility, fileName, line, log);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_enableDebug(JNIEnv *env, jobject thisObj, jint level) {
    mbedtls_debug_set_threshold(level);
    mbedtls_ssl_conf_dbg(&ssl_config, debug_msg, stdout);
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_configureClientCert(JNIEnv *env, jobject thisObj, jbyteArray certificateBytes, jbyteArray keyPair) {
    int cert_len = (*env)->GetArrayLength(env, certificateBytes);
    int key_pair_len = (*env)->GetArrayLength(env, keyPair);
    unsigned char* certificate = (unsigned char*)(*env)->GetByteArrayElements(env, certificateBytes, NULL);
    unsigned char* privateKey = (unsigned char*)(*env)->GetByteArrayElements(env, keyPair, NULL);
    if (mbedtls_x509_crt_parse(&cert_chain1, certificate, (size_t)cert_len) == 0 && mbedtls_pk_parse_key(&key_pair, privateKey, (size_t)key_pair_len, NULL, 0) == 0) {
        mbedtls_ssl_conf_own_cert(&ssl_config, &cert_chain1, &key_pair);
    }
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_configureRootCACert(JNIEnv *env, jobject thisObj, jbyteArray certificateBytes) {
    int len = (*env)->GetArrayLength(env, certificateBytes);
    unsigned char* certificate = (unsigned char*)(*env)->GetByteArrayElements(env, certificateBytes, NULL);
    if (mbedtls_x509_crt_parse(&cert_chain2, certificate, (size_t)len) == 0) {
        mbedtls_ssl_conf_ca_chain(&ssl_config, &cert_chain2, NULL);
    }
}

JNIEXPORT jbyteArray JNICALL Java_com_simplisafe_mbedtls_mbedTLS_getIssuerName(JNIEnv *env, jobject thisObj, jbyteArray certificateBytes) {
    int len = (*env)->GetArrayLength(env, certificateBytes);
    unsigned char* certificate = (unsigned char*)(*env)->GetByteArrayElements(env, certificateBytes, NULL);
    if (mbedtls_x509_crt_parse(&cert_chain3, certificate, (size_t)len) == 0) {
        jbyteArray arr = (*env)->NewByteArray(env, 20);
        (*env)->SetByteArrayRegion(env, arr, 0, 20, (jbyte*)cert_chain3.issuer.next->next->next->next->next->val.p);
        return arr;
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_com_simplisafe_mbedtls_mbedTLS_fixPeerCert(JNIEnv *env, jobject thisObj) {
    ssl_context.session_negotiate->peer_cert = ssl_context.session_negotiate->peer_cert->next;
}