// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/bidirectional_stream_spdy_impl.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = arraysize(kBodyData);
// Size of the buffer to be allocated for each read.
const size_t kReadBufferSize = 4096;

class TestDelegateBase : public BidirectionalStreamImpl::Delegate {
 public:
  TestDelegateBase(base::WeakPtr<SpdySession> session,
                   IOBuffer* read_buf,
                   int read_buf_len)
      : stream_(new BidirectionalStreamSpdyImpl(session)),
        read_buf_(read_buf),
        read_buf_len_(read_buf_len),
        loop_(nullptr),
        error_(OK),
        bytes_read_(0),
        on_data_read_count_(0),
        on_data_sent_count_(0),
        do_not_start_read_(false),
        run_until_completion_(false),
        not_expect_callback_(false),
        on_failed_called_(false) {}

  ~TestDelegateBase() override {}

  void OnStreamReady(bool request_headers_sent) override {
    CHECK(!on_failed_called_);
  }

  void OnHeadersReceived(const SpdyHeaderBlock& response_headers) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    response_headers_ = response_headers.Clone();
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataRead(int bytes_read) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    on_data_read_count_++;
    CHECK_GE(bytes_read, OK);
    bytes_read_ += bytes_read;
    data_received_.append(read_buf_->data(), bytes_read);
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataSent() override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    on_data_sent_count_++;
  }

  void OnTrailersReceived(const SpdyHeaderBlock& trailers) override {
    CHECK(!on_failed_called_);
    trailers_ = trailers.Clone();
    if (run_until_completion_)
      loop_->Quit();
  }

  void OnFailed(int error) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    CHECK_NE(OK, error);
    error_ = error;
    on_failed_called_ = true;
    if (run_until_completion_)
      loop_->Quit();
  }

  void Start(const BidirectionalStreamRequestInfo* request,
             const BoundNetLog& net_log) {
    stream_->Start(request, net_log,
                   /*send_request_headers_automatically=*/false, this,
                   base::WrapUnique(new base::Timer(false, false)));
    not_expect_callback_ = false;
  }

  void SendData(IOBuffer* data, int length, bool end_of_stream) {
    not_expect_callback_ = true;
    stream_->SendData(data, length, end_of_stream);
    not_expect_callback_ = false;
  }

  void SendvData(const std::vector<scoped_refptr<IOBuffer>>& data,
                 const std::vector<int>& length,
                 bool end_of_stream) {
    not_expect_callback_ = true;
    stream_->SendvData(data, length, end_of_stream);
    not_expect_callback_ = false;
  }

  // Sets whether the delegate should wait until the completion of the stream.
  void SetRunUntilCompletion(bool run_until_completion) {
    run_until_completion_ = run_until_completion;
    loop_.reset(new base::RunLoop);
  }

  // Starts or continues read data from |stream_| until there is no more
  // byte can be read synchronously.
  void StartOrContinueReading() {
    int rv = ReadData();
    while (rv > 0) {
      rv = ReadData();
    }
    if (run_until_completion_ && rv == 0)
      loop_->Quit();
  }

  // Calls ReadData on the |stream_| and updates internal states.
  int ReadData() {
    int rv = stream_->ReadData(read_buf_.get(), read_buf_len_);
    if (rv > 0) {
      data_received_.append(read_buf_->data(), rv);
      bytes_read_ += rv;
    }
    return rv;
  }

  NextProto GetProtocol() const { return stream_->GetProtocol(); }

  int64_t GetTotalReceivedBytes() const {
    return stream_->GetTotalReceivedBytes();
  }

  int64_t GetTotalSentBytes() const { return stream_->GetTotalSentBytes(); }

  // Const getters for internal states.
  const std::string& data_received() const { return data_received_; }
  int bytes_read() const { return bytes_read_; }
  int error() const { return error_; }
  const SpdyHeaderBlock& response_headers() const { return response_headers_; }
  const SpdyHeaderBlock& trailers() const { return trailers_; }
  int on_data_read_count() const { return on_data_read_count_; }
  int on_data_sent_count() const { return on_data_sent_count_; }
  bool on_failed_called() const { return on_failed_called_; }

  // Sets whether the delegate should automatically start reading.
  void set_do_not_start_read(bool do_not_start_read) {
    do_not_start_read_ = do_not_start_read;
  }

  // Cancels |stream_|.
  void CancelStream() { stream_->Cancel(); }

 private:
  std::unique_ptr<BidirectionalStreamSpdyImpl> stream_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  std::string data_received_;
  std::unique_ptr<base::RunLoop> loop_;
  SpdyHeaderBlock response_headers_;
  SpdyHeaderBlock trailers_;
  int error_;
  int bytes_read_;
  int on_data_read_count_;
  int on_data_sent_count_;
  bool do_not_start_read_;
  bool run_until_completion_;
  bool not_expect_callback_;
  bool on_failed_called_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegateBase);
};

}  // namespace

class BidirectionalStreamSpdyImplTest : public testing::Test {
 public:
  BidirectionalStreamSpdyImplTest()
      : default_url_(kDefaultUrl),
        host_port_pair_(HostPortPair::FromURL(default_url_)),
        key_(host_port_pair_, ProxyServer::Direct(), PRIVACY_MODE_DISABLED),
        ssl_data_(SSLSocketDataProvider(ASYNC, OK)) {
    ssl_data_.SetNextProto(kProtoHTTP2);
    ssl_data_.cert = ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  }

 protected:
  void TearDown() override {
    if (sequenced_data_) {
      EXPECT_TRUE(sequenced_data_->AllReadDataConsumed());
      EXPECT_TRUE(sequenced_data_->AllWriteDataConsumed());
    }
  }

  // Initializes the session using SequencedSocketData.
  void InitSession(MockRead* reads,
                   size_t reads_count,
                   MockWrite* writes,
                   size_t writes_count) {
    ASSERT_TRUE(ssl_data_.cert.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data_);
    sequenced_data_.reset(
        new SequencedSocketData(reads, reads_count, writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(sequenced_data_.get());
    session_deps_.net_log = net_log_.bound().net_log();
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ =
        CreateSecureSpdySession(http_session_.get(), key_, net_log_.bound());
  }

  BoundTestNetLog net_log_;
  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  const GURL default_url_;
  const HostPortPair host_port_pair_;
  const SpdySessionKey key_;
  std::unique_ptr<SequencedSocketData> sequenced_data_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;

 private:
  SSLSocketDataProvider ssl_data_;
};

TEST_F(BidirectionalStreamSpdyImplTest, SendDataAfterStreamFailed) {
  SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOW, nullptr, 0));
  SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 2),
  };

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3),
  };

  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  std::unique_ptr<TestDelegateBase> delegate(
      new TestDelegateBase(session_, read_buffer.get(), kReadBufferSize));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_.bound());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate->on_failed_called());

  // Try to send data after OnFailed(), should not get called back.
  scoped_refptr<StringIOBuffer> buf(new StringIOBuffer("dummy"));
  delegate->SendData(buf.get(), buf->size(), false);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate->error(), IsError(ERR_SPDY_PROTOCOL_ERROR));
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // BidirectionalStreamSpdyStreamJob does not count the bytes sent for |rst|
  // because it is sent after SpdyStream::Delegate::OnClose is called.
  EXPECT_EQ(CountWriteBytes(writes, 1), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamSpdyImplTest, SendDataAfterCancelStream) {
  SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyData, kBodyDataSize,
                                        /*fin=*/false));
  SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame, 3),
      CreateMockWrite(rst, 5),
  };

  SpdySerializedFrame resp(spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      MockRead(ASYNC, 0, 6),
  };

  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.priority = LOWEST;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  std::unique_ptr<TestDelegateBase> delegate(
      new TestDelegateBase(session_, read_buffer.get(), kReadBufferSize));
  delegate->set_do_not_start_read(true);
  delegate->Start(&request_info, net_log_.bound());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());

  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(kBodyData, kBodyDataSize)));
  delegate->SendData(buf.get(), buf->size(), false);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  // Cancel the stream.
  delegate->CancelStream();
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  // Try to send data after Cancel(), should not get called back.
  delegate->SendData(buf.get(), buf->size(), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate->on_failed_called());

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
}

}  // namespace net
