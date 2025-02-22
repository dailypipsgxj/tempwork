// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Filter performs filtering on data streams. Sample usage:
//
//   IStream* pre_filter_source;
//   ...
//   std::unique_ptr<Filter> filter = Filter::Factory(filter_type, size);
//   int pre_filter_data_len = filter->stream_buffer_size();
//   pre_filter_source->read(filter->stream_buffer(), pre_filter_data_len);
//
//   filter->FlushStreamBuffer(pre_filter_data_len);
//
//   char post_filter_buf[kBufferSize];
//   int post_filter_data_len = kBufferSize;
//   filter->ReadFilteredData(post_filter_buf, &post_filter_data_len);
//
// To filter a data stream, the caller first gets filter's stream_buffer_
// through its accessor and fills in stream_buffer_ with pre-filter data, next
// calls FlushStreamBuffer to notify Filter, then calls ReadFilteredData
// repeatedly to get all the filtered data. After all data have been filtered
// and read out, the caller may fill in stream_buffer_ again. This
// WriteBuffer-Flush-Read cycle is repeated until reaching the end of data
// stream.
//
// A return of FILTER_OK from ReadData() means that more data is
// available to a future ReadData() call and data may not be written
// into stream_buffer(). A return of FILTER_NEED_MORE_DATA from ReadData()
// indicates that no data will be forthcoming from the filter until
// it receives more input data, and that the buffer at
// stream_buffer() may be written to.
//
// The filter being complete (no more data to provide) may be indicated
// by either returning FILTER_DONE or by returning FILTER_OK and indicating
// zero bytes output; consumers understand both those signals. Consumers
// are responsible for not calling ReadData() on a filter after one of these
// signals have been returned. Note that some filters may never signal that
// they are done (e.g. a pass-through filter will always
// say FILTER_NEED_MORE_DATA), so the consumer will also need to
// recognize the state of |no_more_input_data_available &&
// filter->stream_data_len() == 0| as FILTER_DONE.
//
// The lifetime of a Filter instance is completely controlled by its caller.

#ifndef NET_FILTER_FILTER_H__
#define NET_FILTER_FILTER_H__

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/sdch_manager.h"

class GURL;

namespace net {

class BoundNetLog;
class IOBuffer;
class URLRequestContext;

//------------------------------------------------------------------------------
// Define an interface class that allows access to contextual information
// supplied by the owner of this filter. In the case where there are a chain of
// filters, there is only one owner of all the chained filters, and that context
// is passed to the constructor of all those filters. To be clear, the context
// does NOT reflect the position in a chain, or the fact that there are prior
// or later filters in a chain.
//
// TODO(rdsmith): FilterContext is a grab-bag of methods which may or may
// not be relevant for any particular filter, and it's getting worse over
// time. In addition, it only supports two filters, SDCH and gzip.
// It would make more sense to implement FilterContext as a
// base::SupportsUserData structure to which filter-specific information
// could be added by whatever the ultimate consumer of the filter chain is,
// and a particular filter (if included) could access that information.
class NET_EXPORT_PRIVATE FilterContext {
 public:
  // Enum to control what histograms are emitted near end-of-life of this
  // instance.
  enum StatisticSelector {
    SDCH_DECODE,
    SDCH_PASSTHROUGH,
    SDCH_EXPERIMENT_DECODE,
    SDCH_EXPERIMENT_HOLDBACK,
  };

  virtual ~FilterContext();

  // What mime type was specified in the header for this data?
  // Only makes senses for some types of contexts, and returns false
  // when not applicable.
  virtual bool GetMimeType(std::string* mime_type) const = 0;

  // What URL was used to access this data?
  // Return false if gurl is not present.
  virtual bool GetURL(GURL* gurl) const = 0;

  // When was this data requested from a server?
  virtual base::Time GetRequestTime() const = 0;

  // Is data supplied from cache, or fresh across the net?
  virtual bool IsCachedContent() const = 0;

  // Was this data flagged as a response to a request with an SDCH dictionary?
  virtual SdchManager::DictionarySet* SdchDictionariesAdvertised() const = 0;

  // How many bytes were read from the net or cache so far (and potentially
  // pushed into a filter for processing)?
  virtual int64_t GetByteReadCount() const = 0;

  // What response code was received with the associated network transaction?
  // For example: 200 is ok.  4xx are error codes. etc.
  virtual int GetResponseCode() const = 0;

  // The URLRequestContext associated with the request.
  virtual const URLRequestContext* GetURLRequestContext() const = 0;

  // The following method forces the context to emit a specific set of
  // statistics as selected by the argument.
  virtual void RecordPacketStats(StatisticSelector statistic) const = 0;

  // The BoundNetLog of the associated request.
  virtual const BoundNetLog& GetNetLog() const = 0;
};

//------------------------------------------------------------------------------
class NET_EXPORT_PRIVATE Filter {
 public:
  // Return values of function ReadFilteredData.
  enum FilterStatus {
    // Read filtered data successfully
    FILTER_OK,
    // Read filtered data successfully, and the data in the buffer has been
    // consumed by the filter, but more data is needed in order to continue
    // filtering. At this point, the caller is free to reuse the filter
    // buffer to provide more data.
    FILTER_NEED_MORE_DATA,
    // Read filtered data successfully, and filter reaches the end of the data
    // stream.
    FILTER_DONE,
    // There is an error during filtering.
    FILTER_ERROR
  };

  // Specifies type of filters that can be created.  Do not change the values
  // of this enum; it is preserved in a histogram.
  enum FilterType {
    FILTER_TYPE_BROTLI,
    FILTER_TYPE_DEFLATE,
    FILTER_TYPE_GZIP,
    FILTER_TYPE_GZIP_HELPING_SDCH,  // Gzip possible, but pass through allowed.
    FILTER_TYPE_SDCH,
    FILTER_TYPE_SDCH_POSSIBLE,  // Sdch possible, but pass through allowed.
    FILTER_TYPE_UNSUPPORTED,

    FILTER_TYPE_MAX
  };

  virtual ~Filter();

  // Creates a Filter object.
  // Parameters: Filter_types specifies the type of filter created;
  // filter_context allows filters to acquire additional details needed for
  // construction and operation, such as a specification of requisite input
  // buffer size.
  // If success, the function returns the pointer to the Filter object created.
  // If failed or a filter is not needed, the function returns NULL.
  //
  // Note: filter_types is an array of filter types (content encoding types as
  // provided in an HTTP header), which will be chained together serially to do
  // successive filtering of data. The types in the vector are ordered based on
  // encoding order, and the filters are chained to operate in the reverse
  // (decoding) order. For example, types[0] = FILTER_TYPE_SDCH,
  // types[1] = FILTER_TYPE_GZIP will cause data to first be gunzip filtered,
  // and the resulting output from that filter will be sdch decoded.
  static std::unique_ptr<Filter> Factory(
      const std::vector<FilterType>& filter_types,
      const FilterContext& filter_context);

  // A simpler version of Factory() which creates a single, unchained
  // Filter of type FILTER_TYPE_GZIP, or NULL if the filter could not be
  // initialized.
  static std::unique_ptr<Filter> GZipFactory();

  // External call to obtain data from this filter chain. If ther is no
  // next_filter_, then it obtains data from this specific filter.
  FilterStatus ReadData(char* dest_buffer, int* dest_len);

  // Returns a pointer to the stream_buffer_.
  IOBuffer* stream_buffer() const { return stream_buffer_.get(); }

  // Returns the maximum size of stream_buffer_ in number of chars.
  int stream_buffer_size() const { return stream_buffer_size_; }

  // Returns the total number of chars remaining in stream_buffer_ to be
  // filtered.
  //
  // If the function returns 0 then all data has been filtered, and the caller
  // is safe to copy new data into stream_buffer_.
  int stream_data_len() const { return stream_data_len_; }

  // Flushes stream_buffer_ for next round of filtering. After copying data to
  // stream_buffer_, the caller should call this function to notify Filter to
  // start filtering. Then after this function is called, the caller can get
  // post-filtered data using ReadFilteredData. The caller must not write to
  // stream_buffer_ and call this function again before stream_buffer_ is
  // emptied out by ReadFilteredData.
  //
  // The input stream_data_len is the length (in number of chars) of valid
  // data in stream_buffer_. It can not be greater than stream_buffer_size_.
  // The function returns true if success, and false otherwise.
  bool FlushStreamBuffer(int stream_data_len);

  // Translate the text of a filter name (from Content-Encoding header) into a
  // FilterType.
  static FilterType ConvertEncodingToType(const std::string& filter_type);

  // Given a array of encoding_types, try to do some error recovery adjustment
  // to the list. This includes handling known bugs in the Apache server (where
  // redundant gzip encoding is specified), as well as issues regarding SDCH
  // encoding, where various proxies and anti-virus products modify or strip the
  // encodings. These fixups require context, which includes whether this
  // response was made to an SDCH request (i.e., an available dictionary was
  // advertised in the GET), as well as the mime type of the content.
  static void FixupEncodingTypes(const FilterContext& filter_context,
                                 std::vector<FilterType>* encoding_types);

  // Returns a string describing the FilterTypes implemented by this filter.
  std::string OrderedFilterList() const;

  FilterType type() const { return type_id_; }

 protected:
  friend class BrotliUnitTest;
  friend class GZipUnitTest;
  friend class SdchFilterChainingTest;
  FRIEND_TEST_ALL_PREFIXES(FilterTest, ThreeFilterChain);

  explicit Filter(FilterType type_id);

  // Filters the data stored in stream_buffer_ and writes the output into the
  // dest_buffer passed in.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // stream_buffer_. On the other hand, *dest_len can be 0 upon successful
  // return. For example, a decoding filter may process some pre-filter data
  // but not produce output yet.
  virtual FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len) = 0;

  // Copy pre-filter data directly to destination buffer without decoding.
  FilterStatus CopyOut(char* dest_buffer, int* dest_len);

  FilterStatus last_status() const { return last_status_; }

  // Buffer to hold the data to be filtered (the input queue).
  scoped_refptr<IOBuffer> stream_buffer_;

  // Maximum size of stream_buffer_ in number of chars.
  int stream_buffer_size_;

  // Pointer to the next data in stream_buffer_ to be filtered.
  char* next_stream_data_;

  // Total number of remaining chars in stream_buffer_ to be filtered.
  int stream_data_len_;

 private:
  // Allocates and initializes stream_buffer_ and stream_buffer_size_.
  void InitBuffer(int size);

  // A factory helper for creating filters for within a chain of potentially
  // multiple encodings. If a chain of filters is created, then this may be
  // called multiple times during the filter creation process. In most simple
  // cases, this is only called once. Returns NULL and cleans up (deleting
  // filter_list) if a new filter can't be constructed.
  static std::unique_ptr<Filter> PrependNewFilter(
      FilterType type_id,
      const FilterContext& filter_context,
      int buffer_size,
      std::unique_ptr<Filter> filter_list);

  // Helper methods for PrependNewFilter. If initialization is successful,
  // they return a fully initialized Filter. Otherwise, return NULL.
  static std::unique_ptr<Filter> InitBrotliFilter(FilterType type_id,
                                                  int buffer_size);
  static std::unique_ptr<Filter> InitGZipFilter(FilterType type_id,
                                                int buffer_size);
  static std::unique_ptr<Filter> InitSdchFilter(
      FilterType type_id,
      const FilterContext& filter_context,
      int buffer_size);

  // Helper function to empty our output into the next filter's input.
  void PushDataIntoNextFilter();

  // Constructs a filter with an internal buffer of the given size.
  // Only meant to be called by unit tests that need to control the buffer size.
  static std::unique_ptr<Filter> FactoryForTests(
      const std::vector<FilterType>& filter_types,
      const FilterContext& filter_context,
      int buffer_size);

  // An optional filter to process output from this filter.
  std::unique_ptr<Filter> next_filter_;

  // Remember what status or local filter last returned so we can better handle
  // chained filters.
  FilterStatus last_status_;

  // The filter type this filter was constructed from.
  FilterType type_id_;

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

}  // namespace net

#endif  // NET_FILTER_FILTER_H__
