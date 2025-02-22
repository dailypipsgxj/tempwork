// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_memory_data_consumer_handle.h"

#include <stddef.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/child/fixed_received_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
using blink::WebDataConsumerHandle;
using Result = WebDataConsumerHandle::Result;
using Writer = SharedMemoryDataConsumerHandle::Writer;
using BackpressureMode = SharedMemoryDataConsumerHandle::BackpressureMode;
const BackpressureMode kApplyBackpressure =
    SharedMemoryDataConsumerHandle::kApplyBackpressure;
const BackpressureMode kDoNotApplyBackpressure =
    SharedMemoryDataConsumerHandle::kDoNotApplyBackpressure;

const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;
const Result kOk = WebDataConsumerHandle::Ok;
const Result kDone = WebDataConsumerHandle::Done;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

using Checkpoint = StrictMock<MockFunction<void(int)>>;
using ReceivedData = RequestPeer::ReceivedData;

class Logger final : public base::RefCounted<Logger> {
 public:
  Logger() {}
  void Add(const std::string& entry) { log_ += entry + "\n"; }
  const std::string& log() const { return log_; }

 private:
  friend class base::RefCounted<Logger>;
  ~Logger() {}
  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};

class LoggingFixedReceivedData final : public RequestPeer::ReceivedData {
 public:
  LoggingFixedReceivedData(const char* name,
                           const char* s,
                           scoped_refptr<Logger> logger)
      : name_(name), data_(s, s + strlen(s)), logger_(logger) {}
  ~LoggingFixedReceivedData() override {
    logger_->Add(name_ + " is destructed.");
  }

  const char* payload() const override {
    return data_.empty() ? nullptr : &data_[0];
  }
  int length() const override { return static_cast<int>(data_.size()); }
  int encoded_data_length() const override {
    return static_cast<int>(data_.size());
  }
  int encoded_body_length() const override {
    return static_cast<int>(data_.size());
  }

 private:
  const std::string name_;
  const std::vector<char> data_;
  scoped_refptr<Logger> logger_;

  DISALLOW_COPY_AND_ASSIGN(LoggingFixedReceivedData);
};

class DestructionTrackingFunction
    : public base::RefCountedThreadSafe<DestructionTrackingFunction> {
 public:
  MOCK_METHOD0(Destruct, void(void));
  MOCK_METHOD0(Call, void(void));

 protected:
  friend class base::RefCountedThreadSafe<DestructionTrackingFunction>;
  virtual ~DestructionTrackingFunction() { Destruct(); }
};

class MockClient : public WebDataConsumerHandle::Client {
 public:
  MOCK_METHOD0(didGetReadable, void());
};

std::string ToString(const void* p, size_t size) {
  const char* q = static_cast<const char*>(p);
  return std::string(q, q + size);
}

class ThreadedSharedMemoryDataConsumerHandleTest : public ::testing::Test {
 protected:
  class ReadDataOperation;
  class ClientImpl final : public WebDataConsumerHandle::Client {
   public:
    explicit ClientImpl(ReadDataOperation* operation) : operation_(operation) {}

    void didGetReadable() override { operation_->ReadData(); }

   private:
    ReadDataOperation* operation_;
  };

  class ReadDataOperation final {
   public:
    typedef WebDataConsumerHandle::Result Result;
    ReadDataOperation(std::unique_ptr<SharedMemoryDataConsumerHandle> handle,
                      base::MessageLoop* main_message_loop,
                      const base::Closure& on_done)
        : handle_(std::move(handle)),
          main_message_loop_(main_message_loop),
          on_done_(on_done) {}

    const std::string& result() const { return result_; }

    void ReadData() {
      if (!client_) {
        client_.reset(new ClientImpl(this));
        reader_ = handle_->obtainReader(client_.get());
      }

      Result rv = kOk;
      size_t read_size = 0;

      while (true) {
        char buffer[16];
        rv = reader_->read(&buffer, sizeof(buffer), kNone, &read_size);
        if (rv != kOk)
          break;
        result_.insert(result_.size(), &buffer[0], read_size);
      }

      if (rv == kShouldWait) {
        // Wait a while...
        return;
      }

      if (rv != kDone) {
        // Something is wrong.
        result_ = "error";
      }

      // The operation is done.
      reader_.reset();
      main_message_loop_->task_runner()->PostTask(FROM_HERE, on_done_);
    }

   private:
    std::unique_ptr<SharedMemoryDataConsumerHandle> handle_;
    std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
    std::unique_ptr<WebDataConsumerHandle::Client> client_;
    base::MessageLoop* main_message_loop_;
    base::Closure on_done_;
    std::string result_;
  };

  void SetUp() override {
    handle_.reset(
        new SharedMemoryDataConsumerHandle(kApplyBackpressure, &writer_));
  }

  StrictMock<MockClient> client_;
  std::unique_ptr<SharedMemoryDataConsumerHandle> handle_;
  std::unique_ptr<Writer> writer_;
  base::MessageLoop loop_;
};

class SharedMemoryDataConsumerHandleTest
    : public ::testing::TestWithParam<BackpressureMode> {
 protected:
  void SetUp() override {
    handle_.reset(new SharedMemoryDataConsumerHandle(GetParam(), &writer_));
  }
  std::unique_ptr<FixedReceivedData> NewFixedData(const char* s) {
    auto size = strlen(s);
    return base::WrapUnique(new FixedReceivedData(s, size, size, size));
  }

  StrictMock<MockClient> client_;
  std::unique_ptr<SharedMemoryDataConsumerHandle> handle_;
  std::unique_ptr<Writer> writer_;
  base::MessageLoop loop_;
};

void RunPostedTasks() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
}

TEST_P(SharedMemoryDataConsumerHandleTest, ReadFromEmpty) {
  char buffer[4];
  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(buffer, 4, kNone, &read);

  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, AutoClose) {
  char buffer[4];
  size_t read = 88;

  writer_.reset();
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(buffer, 4, kNone, &read);

  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ReadSimple) {
  writer_->AddData(NewFixedData("hello"));

  char buffer[4] = {};
  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(buffer, 3, kNone, &read);

  EXPECT_EQ(kOk, result);
  EXPECT_EQ(3u, read);
  EXPECT_STREQ("hel", buffer);

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, read);
  EXPECT_STREQ("lol", buffer);

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, read);

  writer_->Close();

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ReadAfterHandleIsGone) {
  writer_->AddData(NewFixedData("hello"));

  char buffer[8] = {};
  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);

  handle_.reset();

  Result result = reader->read(buffer, sizeof(buffer), kNone, &read);

  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, read);
  EXPECT_STREQ("hello", buffer);

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, read);

  writer_->Close();

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ReObtainReader) {
  writer_->AddData(NewFixedData("hello"));

  char buffer[4] = {};
  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(buffer, 3, kNone, &read);

  EXPECT_EQ(kOk, result);
  EXPECT_EQ(3u, read);
  EXPECT_STREQ("hel", buffer);

  reader.reset();
  reader = handle_->obtainReader(nullptr);

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, read);
  EXPECT_STREQ("lol", buffer);

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, read);

  writer_->Close();

  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CloseBeforeReading) {
  writer_->AddData(NewFixedData("hello"));
  writer_->Close();

  char buffer[20] = {};
  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(buffer, sizeof(buffer), kNone, &read);

  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, read);
  EXPECT_STREQ("hello", buffer);

  result = reader->read(buffer, sizeof(buffer), kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CloseWithDataBeforeZeroRead) {
  writer_->AddData(NewFixedData("hello"));
  writer_->Close();

  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(nullptr, 0, kNone, &read);

  EXPECT_EQ(kOk, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CloseWithoutDataBeforeZeroRead) {
  writer_->Close();

  size_t read = 88;
  auto reader = handle_->obtainReader(nullptr);
  Result result = reader->read(nullptr, 0, kNone, &read);

  EXPECT_EQ(kDone, result);
}

TEST_P(SharedMemoryDataConsumerHandleTest, AddMultipleData) {
  writer_->AddData(NewFixedData("Once "));
  writer_->AddData(NewFixedData("upon "));
  writer_->AddData(NewFixedData("a "));
  writer_->AddData(NewFixedData("time "));
  writer_->AddData(NewFixedData("there "));
  writer_->AddData(NewFixedData("was "));
  writer_->AddData(NewFixedData("a "));
  writer_->Close();

  char buffer[20];
  size_t read;
  Result result;

  auto reader = handle_->obtainReader(nullptr);
  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 6, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(6u, read);
  EXPECT_STREQ("Once u", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 2, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, read);
  EXPECT_STREQ("po", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 9, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(9u, read);
  EXPECT_STREQ("n a time ", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 3, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(3u, read);
  EXPECT_STREQ("the", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 20, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(9u, read);
  EXPECT_STREQ("re was a ", buffer);

  result = reader->read(buffer, sizeof(buffer), kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, AddMultipleDataInteractively) {
  writer_->AddData(NewFixedData("Once "));
  writer_->AddData(NewFixedData("upon "));

  char buffer[20];
  size_t read;
  Result result;

  auto reader = handle_->obtainReader(nullptr);
  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 6, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(6u, read);
  EXPECT_STREQ("Once u", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 2, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, read);
  EXPECT_STREQ("po", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 9, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, read);
  EXPECT_STREQ("n ", buffer);

  writer_->AddData(NewFixedData("a "));

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 1, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(1u, read);
  EXPECT_STREQ("a", buffer);

  writer_->AddData(NewFixedData("time "));
  writer_->AddData(NewFixedData("there "));
  writer_->AddData(NewFixedData("was "));
  writer_->AddData(NewFixedData("a "));
  writer_->Close();

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 9, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(9u, read);
  EXPECT_STREQ(" time the", buffer);

  std::fill(&buffer[0], &buffer[arraysize(buffer)], 0);
  result = reader->read(buffer, 20, kNone, &read);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(9u, read);
  EXPECT_STREQ("re was a ", buffer);

  result = reader->read(buffer, sizeof(buffer), kNone, &read);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, RegisterClient) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  RunPostedTasks();
  checkpoint.Call(2);
  writer_->Close();
  checkpoint.Call(3);
  RunPostedTasks();
  checkpoint.Call(4);
}

TEST_P(SharedMemoryDataConsumerHandleTest, RegisterClientWhenDataExists) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(0);
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(1);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(2);
  RunPostedTasks();
  checkpoint.Call(3);
}

TEST_P(SharedMemoryDataConsumerHandleTest, AddDataWhenClientIsRegistered) {
  Checkpoint checkpoint;
  char buffer[20];
  Result result;
  size_t size;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(5));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(2);
  writer_->AddData(NewFixedData("upon "));
  checkpoint.Call(3);
  result = reader->read(buffer, sizeof(buffer), kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(10u, size);
  checkpoint.Call(4);
  writer_->AddData(NewFixedData("a "));
  checkpoint.Call(5);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CloseWithClientAndData) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(2);
  writer_->Close();
  checkpoint.Call(3);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ReleaseReader) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  reader.reset();
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(2);
}

TEST_P(SharedMemoryDataConsumerHandleTest, TwoPhaseReadShouldWait) {
  Result result;
  const void* buffer = &result;
  size_t size = 99;

  auto reader = handle_->obtainReader(nullptr);
  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(nullptr, buffer);
  EXPECT_EQ(0u, size);
}

TEST_P(SharedMemoryDataConsumerHandleTest, TwoPhaseReadSimple) {
  writer_->AddData(NewFixedData("Once "));

  Result result;
  const void* buffer = &result;
  size_t size = 99;

  auto reader = handle_->obtainReader(nullptr);
  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, size);
  EXPECT_EQ("Once ", ToString(buffer, 5));

  reader->endRead(1);

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(4u, size);
  EXPECT_EQ("nce ", ToString(buffer, 4));

  reader->endRead(4);

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, size);
  EXPECT_EQ(nullptr, buffer);

  writer_->Close();

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, size);
  EXPECT_EQ(nullptr, buffer);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CallOnClearWhenDestructed1) {
  // Call |on_clear| when the handle is gone and if there is no reader.
  Checkpoint checkpoint;
  scoped_refptr<DestructionTrackingFunction> on_clear(
      new StrictMock<DestructionTrackingFunction>);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*on_clear, Call());
  EXPECT_CALL(*on_clear, Destruct());
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(0);
  handle_.reset(new SharedMemoryDataConsumerHandle(
      kApplyBackpressure,
      base::Bind(&DestructionTrackingFunction::Call, on_clear), &writer_));
  handle_.reset();
  on_clear = nullptr;
  checkpoint.Call(1);
  RunPostedTasks();
  checkpoint.Call(2);
}

TEST_P(SharedMemoryDataConsumerHandleTest, CallOnClearWhenDestructed2) {
  // Call |on_clear| when the reader is gone if the handle is alredy gone.
  Checkpoint checkpoint;
  scoped_refptr<DestructionTrackingFunction> on_clear(
      new StrictMock<DestructionTrackingFunction>);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*on_clear, Call());
  EXPECT_CALL(*on_clear, Destruct());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(0);
  handle_.reset(new SharedMemoryDataConsumerHandle(
      kApplyBackpressure,
      base::Bind(&DestructionTrackingFunction::Call, on_clear), &writer_));
  auto reader = handle_->obtainReader(nullptr);
  handle_.reset();
  on_clear = nullptr;
  checkpoint.Call(1);
  RunPostedTasks();
  checkpoint.Call(2);
  reader.reset();
  checkpoint.Call(3);
  RunPostedTasks();
  checkpoint.Call(4);
}

TEST_P(SharedMemoryDataConsumerHandleTest, DoNotCallOnClearWhenDone) {
  Checkpoint checkpoint;
  scoped_refptr<DestructionTrackingFunction> on_clear(
      new StrictMock<DestructionTrackingFunction>);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*on_clear, Destruct());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(0);
  handle_.reset(new SharedMemoryDataConsumerHandle(
      kApplyBackpressure,
      base::Bind(&DestructionTrackingFunction::Call, on_clear), &writer_));
  on_clear = nullptr;
  checkpoint.Call(1);
  writer_->Close();
  checkpoint.Call(2);
  handle_.reset();
  checkpoint.Call(3);
  RunPostedTasks();
  checkpoint.Call(4);
}

TEST_P(SharedMemoryDataConsumerHandleTest, DoNotCallOnClearWhenErrored) {
  Checkpoint checkpoint;
  scoped_refptr<DestructionTrackingFunction> on_clear(
      new StrictMock<DestructionTrackingFunction>);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*on_clear, Destruct());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(0);
  handle_.reset(new SharedMemoryDataConsumerHandle(
      kApplyBackpressure,
      base::Bind(&DestructionTrackingFunction::Call, on_clear), &writer_));
  on_clear = nullptr;
  checkpoint.Call(1);
  writer_->Fail();
  checkpoint.Call(2);
  handle_.reset();
  checkpoint.Call(3);
  RunPostedTasks();
  checkpoint.Call(4);
}

TEST_P(SharedMemoryDataConsumerHandleTest, TwoPhaseReadWithMultipleData) {
  writer_->AddData(NewFixedData("Once "));
  writer_->AddData(NewFixedData("upon "));

  Result result;
  const void* buffer = &result;
  size_t size = 99;

  auto reader = handle_->obtainReader(nullptr);
  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, size);
  EXPECT_EQ("Once ", ToString(buffer, 5));

  reader->endRead(1);

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(4u, size);
  EXPECT_EQ("nce ", ToString(buffer, 4));

  reader->endRead(4);

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, size);
  EXPECT_EQ("upon ", ToString(buffer, 5));

  reader->endRead(5);

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kShouldWait, result);
  EXPECT_EQ(0u, size);
  EXPECT_EQ(nullptr, buffer);

  writer_->Close();

  result = reader->beginRead(&buffer, kNone, &size);
  EXPECT_EQ(kDone, result);
  EXPECT_EQ(0u, size);
  EXPECT_EQ(nullptr, buffer);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ErrorRead) {
  Checkpoint checkpoint;
  Result result;
  char buffer[20] = {};
  size_t read = 99;
  auto reader = handle_->obtainReader(nullptr);

  writer_->Fail();
  result = reader->read(buffer, sizeof(buffer), kNone, &read);

  EXPECT_EQ(kUnexpectedError, result);
  EXPECT_EQ(0u, read);
}

TEST_P(SharedMemoryDataConsumerHandleTest, ErrorTwoPhaseRead) {
  Result result;
  const void* pointer = &result;
  size_t size = 99;
  auto reader = handle_->obtainReader(nullptr);

  writer_->Fail();
  result = reader->beginRead(&pointer, kNone, &size);

  EXPECT_EQ(kUnexpectedError, result);
  EXPECT_EQ(nullptr, pointer);
  EXPECT_EQ(0u, size);
}

TEST_P(SharedMemoryDataConsumerHandleTest, FailWhileTwoPhaseReadIsInProgress) {
  Result result;
  const void* pointer = nullptr;
  size_t size = 0;
  auto reader = handle_->obtainReader(nullptr);

  writer_->AddData(NewFixedData("Once "));
  result = reader->beginRead(&pointer, kNone, &size);
  auto* buffer = static_cast<const char*>(pointer);

  ASSERT_EQ(kOk, result);
  ASSERT_NE(nullptr, pointer);
  ASSERT_EQ(size, 5u);

  writer_->Fail();

  // We can access the buffer after calling |Fail|. I hope ASAN will detect
  // an error if the region is already freed.
  EXPECT_EQ('O', buffer[0]);
  EXPECT_EQ('n', buffer[1]);
  EXPECT_EQ('c', buffer[2]);
  EXPECT_EQ('e', buffer[3]);
  EXPECT_EQ(' ', buffer[4]);

  EXPECT_EQ(kOk, reader->endRead(size));

  EXPECT_EQ(kUnexpectedError, reader->beginRead(&pointer, kNone, &size));
}

TEST_P(SharedMemoryDataConsumerHandleTest, FailWithClient) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  writer_->Fail();
  checkpoint.Call(2);
  RunPostedTasks();
  checkpoint.Call(3);
}

TEST_P(SharedMemoryDataConsumerHandleTest, FailWithClientAndData) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(2);
  writer_->Fail();
  checkpoint.Call(3);
  RunPostedTasks();
  checkpoint.Call(4);
}

TEST_P(SharedMemoryDataConsumerHandleTest, RecursiveErrorNotification) {
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(client_, didGetReadable())
      .WillOnce(Invoke(writer_.get(), &Writer::Fail));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(client_, didGetReadable());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(0);
  auto reader = handle_->obtainReader(&client_);
  checkpoint.Call(1);
  writer_->AddData(NewFixedData("Once "));
  checkpoint.Call(2);
  RunPostedTasks();
  checkpoint.Call(3);
}

TEST(SharedMemoryDataConsumerHandleBackpressureTest, Read) {
  base::MessageLoop loop;
  char buffer[20];
  Result result;
  size_t size;

  std::unique_ptr<Writer> writer;
  auto handle = base::WrapUnique(
      new SharedMemoryDataConsumerHandle(kApplyBackpressure, &writer));
  scoped_refptr<Logger> logger(new Logger);
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data1", "Once ", logger)));
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data2", "upon ", logger)));
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data3", "a ", logger)));
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data4", "time ", logger)));

  auto reader = handle->obtainReader(nullptr);
  logger->Add("1");
  result = reader->read(buffer, 2, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, size);
  logger->Add("2");
  result = reader->read(buffer, 5, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(5u, size);
  logger->Add("3");
  result = reader->read(buffer, 6, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(6u, size);
  logger->Add("4");

  EXPECT_EQ(
      "1\n"
      "2\n"
      "data1 is destructed.\n"
      "3\n"
      "data2 is destructed.\n"
      "data3 is destructed.\n"
      "4\n",
      logger->log());
}

TEST(SharedMemoryDataConsumerHandleBackpressureTest, CloseAndReset) {
  base::MessageLoop loop;
  char buffer[20];
  Result result;
  size_t size;

  std::unique_ptr<Writer> writer;
  auto handle = base::WrapUnique(
      new SharedMemoryDataConsumerHandle(kApplyBackpressure, &writer));
  scoped_refptr<Logger> logger(new Logger);
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data1", "Once ", logger)));
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data2", "upon ", logger)));
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data3", "a ", logger)));

  auto reader = handle->obtainReader(nullptr);
  logger->Add("1");
  result = reader->read(buffer, 2, kNone, &size);
  EXPECT_EQ(kOk, result);
  EXPECT_EQ(2u, size);
  logger->Add("2");
  writer->Close();
  logger->Add("3");
  handle.reset();
  reader.reset();
  logger->Add("4");

  EXPECT_EQ(
      "1\n"
      "2\n"
      "3\n"
      "data1 is destructed.\n"
      "data2 is destructed.\n"
      "data3 is destructed.\n"
      "4\n",
      logger->log());
}

TEST(SharedMemoryDataConsumerHandleWithoutBackpressureTest, AddData) {
  base::MessageLoop loop;
  std::unique_ptr<Writer> writer;
  auto handle = base::WrapUnique(
      new SharedMemoryDataConsumerHandle(kDoNotApplyBackpressure, &writer));
  scoped_refptr<Logger> logger(new Logger);

  logger->Add("1");
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data1", "Once ", logger)));
  logger->Add("2");
  writer->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data2", "upon ", logger)));
  logger->Add("3");

  EXPECT_EQ(
      "1\n"
      "data1 is destructed.\n"
      "2\n"
      "data2 is destructed.\n"
      "3\n",
      logger->log());
}

TEST_F(ThreadedSharedMemoryDataConsumerHandleTest, Read) {
  base::RunLoop run_loop;
  auto operation = base::WrapUnique(new ReadDataOperation(
      std::move(handle_), &loop_, run_loop.QuitClosure()));
  scoped_refptr<Logger> logger(new Logger);

  base::Thread t("DataConsumerHandle test thread");
  ASSERT_TRUE(t.Start());

  t.task_runner()->PostTask(FROM_HERE,
                            base::Bind(&ReadDataOperation::ReadData,
                                       base::Unretained(operation.get())));

  logger->Add("1");
  writer_->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data1", "Once ", logger)));
  writer_->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data2", "upon ", logger)));
  writer_->AddData(base::WrapUnique(
      new LoggingFixedReceivedData("data3", "a time ", logger)));
  writer_->AddData(base::WrapUnique(
      new LoggingFixedReceivedData("data4", "there ", logger)));
  writer_->AddData(
      base::WrapUnique(new LoggingFixedReceivedData("data5", "was ", logger)));
  writer_->Close();
  logger->Add("2");

  run_loop.Run();
  t.Stop();

  EXPECT_EQ("Once upon a time there was ", operation->result());
  EXPECT_EQ(
      "1\n"
      "2\n"
      "data1 is destructed.\n"
      "data2 is destructed.\n"
      "data3 is destructed.\n"
      "data4 is destructed.\n"
      "data5 is destructed.\n",
      logger->log());
}

INSTANTIATE_TEST_CASE_P(SharedMemoryDataConsumerHandleTest,
                        SharedMemoryDataConsumerHandleTest,
                        ::testing::Values(kApplyBackpressure,
                                          kDoNotApplyBackpressure));
}  // namespace

}  // namespace content
