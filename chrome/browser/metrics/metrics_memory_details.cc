// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_memory_details.h"

#include <stddef.h>

#include <vector>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/process_type.h"

MemoryGrowthTracker::MemoryGrowthTracker() {
}

MemoryGrowthTracker::~MemoryGrowthTracker() {
}

bool MemoryGrowthTracker::UpdateSample(base::ProcessId pid,
                                       int sample,
                                       int* diff) {
  // |sample| is memory usage in kB.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  std::map<base::ProcessId, int>::iterator found_size = memory_sizes_.find(pid);
  if (found_size != memory_sizes_.end()) {
    const int last_size = found_size->second;
    std::map<base::ProcessId, base::TimeTicks>::iterator found_time =
        times_.find(pid);
    const base::TimeTicks last_time = found_time->second;
    if (last_time < (current_time - base::TimeDelta::FromMinutes(30))) {
      // Note that it is undefined how division of a negative integer gets
      // rounded. |*diff| may have a difference of 1 from the correct number
      // if |sample| < |last_size|. We ignore it as 1 is small enough.
      *diff =
          ((sample - last_size) * 30 / (current_time - last_time).InMinutes());
      found_size->second = sample;
      found_time->second = current_time;
      return true;
    }
    // Skip if a last record is found less than 30 minutes ago.
  } else {
    // Not reporting if it's the first record for |pid|.
    times_[pid] = current_time;
    memory_sizes_[pid] = sample;
  }
  return false;
}

MetricsMemoryDetails::MetricsMemoryDetails(
    const base::Closure& callback,
    MemoryGrowthTracker* memory_growth_tracker)
    : callback_(callback), memory_growth_tracker_(memory_growth_tracker) {
  memory_growth_tracker_ = memory_growth_tracker;
}

MetricsMemoryDetails::~MetricsMemoryDetails() {
}

void MetricsMemoryDetails::OnDetailsAvailable() {
  UpdateHistograms();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback_);
}

void MetricsMemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.
  // Memory is measured in KB.

  const ProcessData& browser = *ChromeBrowser();
  size_t aggregate_memory = 0;
  int chrome_count = 0;
  int extension_count = 0;
  int pepper_plugin_count = 0;
  int pepper_plugin_broker_count = 0;
  int renderer_count = 0;
  int other_count = 0;
  int worker_count = 0;
  int process_limit = content::RenderProcessHost::GetMaxRendererProcessCount();
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.Large2", sample / 1024);
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Extension", sample);
            extension_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Chrome", sample);
            chrome_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            // TODO(erikkay): Should we bother splitting out the other subtypes?
            UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Renderer.Large2",
                                          sample / 1024);
            int diff;
            if (memory_growth_tracker_ &&
                memory_growth_tracker_->UpdateSample(
                    browser.processes[index].pid, sample, &diff)) {
              if (diff < 0)
                UMA_HISTOGRAM_MEMORY_KB("Memory.RendererShrinkIn30Min", -diff);
              else
                UMA_HISTOGRAM_MEMORY_KB("Memory.RendererGrowthIn30Min", diff);
            }
            renderer_count++;
            continue;
        }
      }
      case content::PROCESS_TYPE_UTILITY:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Utility", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Zygote", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.SandboxHelper", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_GPU:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Gpu", sample);
        other_count++;
        continue;
#if defined(ENABLE_PLUGINS)
      case content::PROCESS_TYPE_PPAPI_PLUGIN: {
        const std::vector<base::string16>& titles =
            browser.processes[index].titles;
        if (titles.size() == 1 &&
            titles[0] == base::ASCIIToUTF16(content::kFlashPluginName)) {
          UMA_HISTOGRAM_MEMORY_KB("Memory.PepperFlashPlugin", sample);
        }
        UMA_HISTOGRAM_MEMORY_KB("Memory.PepperPlugin", sample);
        pepper_plugin_count++;
        continue;
      }
      case content::PROCESS_TYPE_PPAPI_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.PepperPluginBroker", sample);
        pepper_plugin_broker_count++;
        continue;
#endif
      case PROCESS_TYPE_NACL_LOADER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClient", sample);
        other_count++;
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClientBroker", sample);
        other_count++;
        continue;
      default:
        NOTREACHED();
        continue;
    }
  }
#if defined(OS_CHROMEOS)
  // Chrome OS exposes system-wide graphics driver memory which has historically
  // been a source of leak/bloat.
  base::SystemMemoryInfoKB meminfo;
  if (base::GetSystemMemoryInfo(&meminfo) && meminfo.gem_size != -1)
    UMA_HISTOGRAM_MEMORY_MB("Memory.Graphics", meminfo.gem_size / 1024 / 1024);
#endif

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessLimit", process_limit);
  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
                           static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.ChromeProcessCount", chrome_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.ExtensionProcessCount", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.OtherProcessCount", other_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PepperPluginProcessCount",
                           pepper_plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PepperPluginBrokerProcessCount",
                           pepper_plugin_broker_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.WorkerProcessCount", worker_count);
  // TODO(viettrungluu): Do we want separate counts for the other
  // (platform-specific) process types?

  // TODO(rkaplow): Remove once we've verified Memory.Total2 is ok.
  int total_sample_old = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Total", total_sample_old);
  int total_sample = static_cast<int>(aggregate_memory / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total2", total_sample);

  // Predict the number of processes needed when isolating all sites and when
  // isolating only HTTPS sites.
  int all_renderer_count = renderer_count + chrome_count + extension_count;
  int non_renderer_count = browser.processes.size() - all_renderer_count;
  DCHECK_GE(non_renderer_count, 1);
  SiteDetails::UpdateHistograms(browser.site_data, all_renderer_count,
                                non_renderer_count);

#if defined(OS_CHROMEOS)
  UpdateSwapHistograms();
#endif
}

#if defined(OS_CHROMEOS)
void MetricsMemoryDetails::UpdateSwapHistograms() {
  UMA_HISTOGRAM_BOOLEAN("Memory.Swap.HaveSwapped", swap_info().num_writes > 0);
  if (swap_info().num_writes == 0)
    return;

  // Only record swap info when any swaps have happened, to give us more
  // detail in the histograms.
  const ProcessData& browser = *ChromeBrowser();
  size_t aggregate_memory = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.swapped);
    aggregate_memory += sample;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Browser", sample);
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Extension", sample);
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Chrome", sample);
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Renderer", sample);
            continue;
        }
      }
      case content::PROCESS_TYPE_UTILITY:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Utility", sample);
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Zygote", sample);
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.SandboxHelper", sample);
        continue;
      case content::PROCESS_TYPE_GPU:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Gpu", sample);
        continue;
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.PepperPlugin", sample);
        continue;
      case content::PROCESS_TYPE_PPAPI_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.PepperPluginBroker", sample);
        continue;
      case PROCESS_TYPE_NACL_LOADER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.NativeClient", sample);
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.NativeClientBroker", sample);
        continue;
      default:
        NOTREACHED();
        continue;
    }
  }

  // TODO(rkaplow): Remove once we've verified Memory.Swap.Total2 is ok.
  int total_sample_old = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Swap.Total", total_sample_old);
  int total_sample = static_cast<int>(aggregate_memory / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Swap.Total2", total_sample);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.CompressedDataSize",
                              swap_info().compr_data_size / (1024 * 1024), 1,
                              4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.OriginalDataSize",
                              swap_info().orig_data_size / (1024 * 1024), 1,
                              4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.MemUsedTotal",
                              swap_info().mem_used_total / (1024 * 1024), 1,
                              4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.NumReads", swap_info().num_reads, 1,
                              100000000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.NumWrites", swap_info().num_writes,
                              1, 100000000, 100);

  if (swap_info().orig_data_size > 0 && swap_info().compr_data_size > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Memory.Swap.CompressionRatio",
        swap_info().orig_data_size / swap_info().compr_data_size, 1, 20, 20);
  }
}
#endif  // defined(OS_CHROMEOS)
