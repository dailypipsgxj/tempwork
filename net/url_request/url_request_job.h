// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/cookies/canonical_cookie.h"
#include "net/socket/connection_attempts.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class CookieOptions;
class Filter;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class NetworkDelegate;
class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
class UploadDataStream;
class URLRequestStatus;
class X509Certificate;

class NET_EXPORT URLRequestJob : public base::PowerObserver {
 public:
  explicit URLRequestJob(URLRequest* request,
                         NetworkDelegate* network_delegate);
  ~URLRequestJob() override;

  // Returns the request that owns this job.
  URLRequest* request() const {
    return request_;
  }

  // Sets the upload data, most requests have no upload data, so this is a NOP.
  // Job types supporting upload data will override this.
  virtual void SetUpload(UploadDataStream* upload_data_stream);

  // Sets extra request headers for Job types that support request
  // headers. Called once before Start() is called.
  virtual void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  // Sets the priority of the job. Called once before Start() is
  // called, but also when the priority of the parent request changes.
  virtual void SetPriority(RequestPriority priority);

  // If any error occurs while starting the Job, NotifyStartError should be
  // called.
  // This helps ensure that all errors follow more similar notification code
  // paths, which should simplify testing.
  virtual void Start() = 0;

  // This function MUST somehow call NotifyDone/NotifyCanceled or some requests
  // will get leaked. Certain callers use that message to know when they can
  // delete their URLRequest object, even when doing a cancel. The default
  // Kill implementation calls NotifyCanceled, so it is recommended that
  // subclasses call URLRequestJob::Kill() after doing any additional work.
  //
  // The job should endeavor to stop working as soon as is convenient, but must
  // not send and complete notifications from inside this function. Instead,
  // complete notifications (including "canceled") should be sent from a
  // callback run from the message loop.
  //
  // The job is not obliged to immediately stop sending data in response to
  // this call, nor is it obliged to fail with "canceled" unless not all data
  // was sent as a result. A typical case would be where the job is almost
  // complete and can succeed before the canceled notification can be
  // dispatched (from the message loop).
  //
  // The job should be prepared to receive multiple calls to kill it, but only
  // one notification must be issued.
  virtual void Kill();

  // Called to read post-filtered data from this Job, returning the number of
  // bytes read, 0 when there is no more data, or -1 if there was an error.
  // This is just the backend for URLRequest::Read, see that function for
  // more info.
  bool Read(IOBuffer* buf, int buf_size, int* bytes_read);

  // Stops further caching of this request, if any. For more info, see
  // URLRequest::StopCaching().
  virtual void StopCaching();

  virtual bool GetFullRequestHeaders(HttpRequestHeaders* headers) const;

  // Get the number of bytes received from network. The values returned by this
  // will never decrease over the lifetime of the URLRequestJob.
  virtual int64_t GetTotalReceivedBytes() const;

  // Get the number of bytes sent over the network. The values returned by this
  // will never decrease over the lifetime of the URLRequestJob.
  virtual int64_t GetTotalSentBytes() const;

  // Called to fetch the current load state for the job.
  virtual LoadState GetLoadState() const;

  // Called to get the upload progress in bytes.
  virtual UploadProgress GetUploadProgress() const;

  // Called to fetch the charset for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a charset will return false.
  virtual bool GetCharset(std::string* charset);

  // Called to get response info.
  virtual void GetResponseInfo(HttpResponseInfo* info);

  // This returns the times when events actually occurred, rather than the time
  // each event blocked the request.  See FixupLoadTimingInfo in url_request.h
  // for more information on the difference.
  virtual void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Gets the remote endpoint that the network stack is currently fetching the
  // URL from. Returns true and fills in |endpoint| if it is available; returns
  // false and leaves |endpoint| unchanged if it is unavailable.
  virtual bool GetRemoteEndpoint(IPEndPoint* endpoint) const;

  // Populates the network error details of the most recent origin that the
  // network stack makes the request to.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // Called to setup a stream filter for this request. An example of filter is
  // content encoding/decoding.
  // Subclasses should return the appropriate Filter, or NULL for no Filter.
  // This class takes ownership of the returned Filter.
  //
  // The default implementation returns NULL.
  virtual std::unique_ptr<Filter> SetupFilter() const;

  // Called to determine if this response is a redirect.  Only makes sense
  // for some types of requests.  This method returns true if the response
  // is a redirect, and fills in the location param with the URL of the
  // redirect.  The HTTP status code (e.g., 302) is filled into
  // |*http_status_code| to signify the type of redirect.
  //
  // The caller is responsible for following the redirect by setting up an
  // appropriate replacement Job. Note that the redirected location may be
  // invalid, the caller should be sure it can handle this.
  //
  // The default implementation inspects the response_info_.
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // Called to determine if it is okay to copy the reference fragment from the
  // original URL (if existent) to the redirection target when the redirection
  // target has no reference fragment.
  //
  // The default implementation returns true.
  virtual bool CopyFragmentOnRedirect(const GURL& location) const;

  // Called to determine if it is okay to redirect this job to the specified
  // location.  This may be used to implement protocol-specific restrictions.
  // If this function returns false, then the URLRequest will fail
  // reporting ERR_UNSAFE_REDIRECT.
  virtual bool IsSafeRedirect(const GURL& location);

  // Called to determine if this response is asking for authentication.  Only
  // makes sense for some types of requests.  The caller is responsible for
  // obtaining the credentials passing them to SetAuth.
  virtual bool NeedsAuth();

  // Fills the authentication info with the server's response.
  virtual void GetAuthChallengeInfo(
      scoped_refptr<AuthChallengeInfo>* auth_info);

  // Resend the request with authentication credentials.
  virtual void SetAuth(const AuthCredentials& credentials);

  // Display the error page without asking for credentials again.
  virtual void CancelAuth();

  virtual void ContinueWithCertificate(X509Certificate* client_cert,
                                       SSLPrivateKey* client_private_key);

  // Continue processing the request ignoring the last error.
  virtual void ContinueDespiteLastError();

  // Continue with the network request.
  virtual void ResumeNetworkStart();

  void FollowDeferredRedirect();

  // Returns true if the Job is done producing response data and has called
  // NotifyDone on the request.
  bool is_done() const { return done_; }

  // Get/Set expected content size
  int64_t expected_content_size() const { return expected_content_size_; }
  void set_expected_content_size(const int64_t& size) {
    expected_content_size_ = size;
  }

  // Whether we have processed the response for that request yet.
  bool has_response_started() const { return has_handled_response_; }

  // The number of bytes read before passing to the filter. This value reflects
  // bytes read even when there is no filter.
  int64_t prefilter_bytes_read() const { return prefilter_bytes_read_; }

  // These methods are not applicable to all connections.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual int GetResponseCode() const;

  // Returns the socket address for the connection.
  // See url_request.h for details.
  virtual HostPortPair GetSocketAddress() const;

  // base::PowerObserver methods:
  // We invoke URLRequestJob::Kill on suspend (crbug.com/4606).
  void OnSuspend() override;

  // Called after a NetworkDelegate has been informed that the URLRequest
  // will be destroyed. This is used to track that no pending callbacks
  // exist at destruction time of the URLRequestJob, unless they have been
  // canceled by an explicit NetworkDelegate::NotifyURLRequestDestroyed() call.
  virtual void NotifyURLRequestDestroyed();

  // Populates |out| with the connection attempts made at the socket layer in
  // the course of executing the URLRequestJob. Should be called after the job
  // has failed or the response headers have been received.
  virtual void GetConnectionAttempts(ConnectionAttempts* out) const;

  // Given |policy|, |referrer|, and |redirect_destination|, returns the
  // referrer URL mandated by |request|'s referrer policy.
  static GURL ComputeReferrerForRedirect(URLRequest::ReferrerPolicy policy,
                                         const std::string& referrer,
                                         const GURL& redirect_destination);

 protected:
  // Notifies the job that a certificate is requested.
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);

  // Notifies the job about an SSL certificate error.
  void NotifySSLCertificateError(const SSLInfo& ssl_info, bool fatal);

  // Delegates to URLRequest::Delegate.
  bool CanGetCookies(const CookieList& cookie_list) const;

  // Delegates to URLRequest::Delegate.
  bool CanSetCookie(const std::string& cookie_line,
                    CookieOptions* options) const;

  // Delegates to URLRequest::Delegate.
  bool CanEnablePrivacyMode() const;

  // Notifies the job that headers have been received.
  void NotifyHeadersComplete();

  // Notifies the request that a start error has occurred.
  void NotifyStartError(const URLRequestStatus& status);

  // Used as an asynchronous callback for Kill to notify the URLRequest
  // that we were canceled.
  void NotifyCanceled();

  // Notifies the job the request should be restarted.
  // Should only be called if the job has not started a response.
  void NotifyRestartRequired();

  // See corresponding functions in url_request.h.
  void OnCallToDelegate();
  void OnCallToDelegateComplete();

  // Called to read raw (pre-filtered) data from this Job. Reads at most
  // |buf_size| bytes into |buf|.
  // Possible return values:
  //   >= 0: Read completed synchronously. Return value is the number of bytes
  //         read. 0 means eof.
  //   ERR_IO_PENDING: Read pending asynchronously.
  //                   When the read completes, |ReadRawDataComplete| should be
  //                   called.
  //   Any other negative number: Read failed synchronously. Return value is a
  //                   network error code.
  // This method might hold onto a reference to |buf| (by incrementing the
  // refcount) until the method completes or is cancelled.
  virtual int ReadRawData(IOBuffer* buf, int buf_size);

  // Called to tell the job that a filter has successfully reached the end of
  // the stream.
  virtual void DoneReading();

  // Called to tell the job that the body won't be read because it's a redirect.
  // This is needed so that redirect headers can be cached even though their
  // bodies are never read.
  virtual void DoneReadingRedirectResponse();

  // Reads filtered data from the request. Returns OK if immediately successful,
  // ERR_IO_PENDING if the request couldn't complete synchronously, and some
  // other error code if the request failed synchronously. Note that this
  // function can issue new asynchronous requests if needed, in which case it
  // returns ERR_IO_PENDING. If this method completes synchronously,
  // |*bytes_read| is the number of bytes output by the filter chain if this
  // method returns OK, or zero if this method returns an error.
  Error ReadFilteredData(int* bytes_read);

  // Whether the response is being filtered in this job.
  // Only valid after NotifyHeadersComplete() has been called.
  bool HasFilter() { return filter_ != NULL; }

  // At or near destruction time, a derived class may request that the filters
  // be destroyed so that statistics can be gathered while the derived class is
  // still present to assist in calculations.  This is used by URLRequestHttpJob
  // to get SDCH to emit stats.
  void DestroyFilters();

  // Provides derived classes with access to the request's network delegate.
  NetworkDelegate* network_delegate() { return network_delegate_; }

  // The status of the job.
  const URLRequestStatus GetStatus();

  // Set the proxy server that was used, if any.
  void SetProxyServer(const HostPortPair& proxy_server);

  // The number of bytes read after passing through the filter. This value
  // reflects bytes read even when there is no filter.
  int64_t postfilter_bytes_read() const { return postfilter_bytes_read_; }

  // Turns an integer result code into an Error and a count of bytes read.
  // The semantics are:
  //   |result| >= 0: |*error| == OK, |*count| == |result|
  //   |result| < 0: |*error| = |result|, |*count| == 0
  static void ConvertResultToError(int result, Error* error, int* count);

  // Completion callback for raw reads. See |ReadRawData| for details.
  // |bytes_read| is either >= 0 to indicate a successful read and count of
  // bytes read, or < 0 to indicate an error.
  void ReadRawDataComplete(int bytes_read);

  // The request that initiated this job. This value will never be nullptr.
  URLRequest* request_;

 private:
  // Set the status of the associated URLRequest.
  // TODO(mmenke): Make the URLRequest manage its own status.
  void SetStatus(const URLRequestStatus& status);

  // When data filtering is enabled, this function is used to read data
  // for the filter. Returns a net error code to indicate if raw data was
  // successfully read,  an error happened, or the IO is pending.
  Error ReadRawDataForFilter(int* bytes_read);

  // Informs the filter chain that data has been read into its buffer.
  void PushInputToFilter(int bytes_read);

  // Invokes ReadRawData and records bytes read if the read completes
  // synchronously.
  Error ReadRawDataHelper(IOBuffer* buf, int buf_size, int* bytes_read);

  // Called in response to a redirect that was not canceled to follow the
  // redirect. The current job will be replaced with a new job loading the
  // given redirect destination.
  void FollowRedirect(const RedirectInfo& redirect_info);

  // Called after every raw read. If |bytes_read| is > 0, this indicates
  // a successful read of |bytes_read| unfiltered bytes. If |bytes_read|
  // is 0, this indicates that there is no additional data to read. |error|
  // specifies whether an error occurred and no bytes were read.
  void GatherRawReadStats(Error error, int bytes_read);

  // Updates the profiling info and notifies observers that an additional
  // |bytes_read| unfiltered bytes have been read for this job.
  void RecordBytesRead(int bytes_read);

  // Called to query whether there is data available in the filter to be read
  // out.
  bool FilterHasData();

  // NotifyDone marks that request is done. It is really a glorified
  // set_status, but also does internal state checking and job tracking. It
  // should be called once per request, when the job is finished doing all IO.
  void NotifyDone(const URLRequestStatus& status);

  // Some work performed by NotifyDone must be completed asynchronously so
  // as to avoid re-entering URLRequest::Delegate. This method performs that
  // work.
  void CompleteNotifyDone();

  // Subclasses may implement this method to record packet arrival times.
  // The default implementation does nothing.  Only invoked when bytes have been
  // read since the last invocation.
  virtual void UpdatePacketReadTimes();

  // Computes a new RedirectInfo based on receiving a redirect response of
  // |location| and |http_status_code|.
  RedirectInfo ComputeRedirectInfo(const GURL& location, int http_status_code);

  // Notify the network delegate that more bytes have been received or sent over
  // the network, if bytes have been received or sent since the previous
  // notification.
  void MaybeNotifyNetworkBytes();

  // Indicates that the job is done producing data, either it has completed
  // all the data or an error has been encountered. Set exclusively by
  // NotifyDone so that it is kept in sync with the request.
  bool done_;

  int64_t prefilter_bytes_read_;
  int64_t postfilter_bytes_read_;

  // The data stream filter which is enabled on demand.
  std::unique_ptr<Filter> filter_;

  // If the filter filled its output buffer, then there is a change that it
  // still has internal data to emit, and this flag is set.
  bool filter_needs_more_output_space_;

  // When we filter data, we receive data into the filter buffers.  After
  // processing the filtered data, we return the data in the caller's buffer.
  // While the async IO is in progress, we save the user buffer here, and
  // when the IO completes, we fill this in.
  scoped_refptr<IOBuffer> filtered_read_buffer_;
  int filtered_read_buffer_len_;

  // We keep a pointer to the read buffer while asynchronous reads are
  // in progress, so we are able to pass those bytes to job observers.
  scoped_refptr<IOBuffer> raw_read_buffer_;

  // Used by HandleResponseIfNecessary to track whether we've sent the
  // OnResponseStarted callback and potentially redirect callbacks as well.
  bool has_handled_response_;

  // Expected content size
  int64_t expected_content_size_;

  // Set when a redirect is deferred.
  RedirectInfo deferred_redirect_info_;

  // The network delegate to use with this request, if any.
  NetworkDelegate* network_delegate_;

  // The value from GetTotalReceivedBytes() the last time
  // MaybeNotifyNetworkBytes() was called. Used to calculate how bytes have been
  // newly received since the last notification.
  int64_t last_notified_total_received_bytes_;

  // The value from GetTotalSentBytes() the last time MaybeNotifyNetworkBytes()
  // was called. Used to calculate how bytes have been newly sent since the last
  // notification.
  int64_t last_notified_total_sent_bytes_;

  base::WeakPtrFactory<URLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_H_
