// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "MainDelegate.h"

int main(int argc, const char* argv[]) {
  MainDelegate* delegate = [[MainDelegate alloc] init];
  [delegate runApplication];

  [[NSRunLoop mainRunLoop] run];
  return 1;
}
