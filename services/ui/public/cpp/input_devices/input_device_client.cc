// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/input_devices/input_device_client.h"

#include "base/logging.h"

namespace ui {

InputDeviceClient::InputDeviceClient() : binding_(this) {
  InputDeviceManager::SetInstance(this);
}

InputDeviceClient::~InputDeviceClient() {
  InputDeviceManager::ClearInstance();
}

void InputDeviceClient::Connect(mojom::InputDeviceServerPtr server) {
  DCHECK(server.is_bound());

  server->AddObserver(binding_.CreateInterfacePtrAndBind());
}

void InputDeviceClient::OnKeyboardDeviceConfigurationChanged(
    const std::vector<ui::InputDevice>& devices) {
  keyboard_devices_ = devices;
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnKeyboardDeviceConfigurationChanged());
}

void InputDeviceClient::OnTouchscreenDeviceConfigurationChanged(
    const std::vector<ui::TouchscreenDevice>& devices) {
  touchscreen_devices_ = devices;
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnTouchscreenDeviceConfigurationChanged());
}

void InputDeviceClient::OnMouseDeviceConfigurationChanged(
    const std::vector<ui::InputDevice>& devices) {
  mouse_devices_ = devices;
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnMouseDeviceConfigurationChanged());
}

void InputDeviceClient::OnTouchpadDeviceConfigurationChanged(
    const std::vector<ui::InputDevice>& devices) {
  touchpad_devices_ = devices;
  FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                    OnTouchpadDeviceConfigurationChanged());
}

void InputDeviceClient::OnDeviceListsComplete(
    const std::vector<ui::InputDevice>& keyboard_devices,
    const std::vector<ui::TouchscreenDevice>& touchscreen_devices,
    const std::vector<ui::InputDevice>& mouse_devices,
    const std::vector<ui::InputDevice>& touchpad_devices) {
  // Update the cached device lists if the received list isn't empty.
  if (!keyboard_devices.empty())
    OnKeyboardDeviceConfigurationChanged(keyboard_devices);
  if (!touchscreen_devices.empty())
    OnTouchscreenDeviceConfigurationChanged(touchscreen_devices);
  if (!mouse_devices.empty())
    OnMouseDeviceConfigurationChanged(mouse_devices);
  if (!touchpad_devices.empty())
    OnTouchpadDeviceConfigurationChanged(touchpad_devices);

  if (!device_lists_complete_) {
    device_lists_complete_ = true;
    FOR_EACH_OBSERVER(ui::InputDeviceEventObserver, observers_,
                      OnDeviceListsComplete());
  }
}

void InputDeviceClient::AddObserver(ui::InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceClient::RemoveObserver(ui::InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetKeyboardDevices()
    const {
  return keyboard_devices_;
}

const std::vector<ui::TouchscreenDevice>&
InputDeviceClient::GetTouchscreenDevices() const {
  return touchscreen_devices_;
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetMouseDevices() const {
  return mouse_devices_;
}

const std::vector<ui::InputDevice>& InputDeviceClient::GetTouchpadDevices()
    const {
  return touchpad_devices_;
}

bool InputDeviceClient::AreDeviceListsComplete() const {
  return device_lists_complete_;
}

bool InputDeviceClient::AreTouchscreensEnabled() const {
  // TODO(kylechar): This obviously isn't right. We either need to pass this
  // state around or modify the interface.
  return true;
}

}  // namespace ui
