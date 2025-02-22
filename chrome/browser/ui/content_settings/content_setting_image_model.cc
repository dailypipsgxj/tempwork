// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"

using content::WebContents;

namespace {

bool UseVectorGraphics() {
  return ui::MaterialDesignController::IsModeMaterial();
}

}  // namespace

// The image models hierarchy:
//
// ContentSettingImageModel                  - base class
//   ContentSettingSimpleImageModel            - single content setting
//     ContentSettingBlockedImageModel           - generic blocked setting
//     ContentSettingGeolocationImageModel       - geolocation
//     ContentSettingRPHImageModel               - protocol handlers
//     ContentSettingMIDISysExImageModel         - midi sysex
//   ContentSettingMediaImageModel             - media
//   ContentSettingSubresourceFilterImageModel - deceptive content

class ContentSettingBlockedImageModel : public ContentSettingSimpleImageModel {
 public:
  explicit ContentSettingBlockedImageModel(ContentSettingsType content_type);

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingBlockedImageModel);
};

class ContentSettingGeolocationImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingGeolocationImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingGeolocationImageModel);
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMediaImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

  ContentSettingBubbleModel* CreateBubbleModel(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents,
      Profile* profile) override;

  bool ShouldRunAnimation(WebContents* web_contents) override;
  void SetAnimationHasRun(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingMediaImageModel);
};

class ContentSettingRPHImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingRPHImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;
};

class ContentSettingMIDISysExImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingMIDISysExImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingMIDISysExImageModel);
};

namespace {

struct ContentSettingsImageDetails {
  ContentSettingsType type;
  int blocked_icon_id;
  gfx::VectorIconId vector_icon_id;
  int blocked_tooltip_id;
  int blocked_explanatory_text_id;
  int accessed_icon_id;
  int accessed_tooltip_id;
};

static const ContentSettingsImageDetails kImageDetails[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES,
     IDR_BLOCKED_COOKIES,
     gfx::VectorIconId::COOKIE,
     IDS_BLOCKED_COOKIES_TITLE,
     0,
     IDR_ACCESSED_COOKIES,
     IDS_ACCESSED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_IMAGES,
     IDR_BLOCKED_IMAGES,
     gfx::VectorIconId::IMAGE,
     IDS_BLOCKED_IMAGES_TITLE,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT,
     IDR_BLOCKED_JAVASCRIPT,
     gfx::VectorIconId::CODE,
     IDS_BLOCKED_JAVASCRIPT_TITLE,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_PLUGINS,
     IDR_BLOCKED_PLUGINS,
     gfx::VectorIconId::EXTENSION,
     IDS_BLOCKED_PLUGINS_MESSAGE,
     IDS_BLOCKED_PLUGIN_EXPLANATORY_TEXT,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_POPUPS,
     IDR_BLOCKED_POPUPS,
     gfx::VectorIconId::WEB,
     IDS_BLOCKED_POPUPS_TOOLTIP,
     IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
     IDR_BLOCKED_MIXED_CONTENT,
     gfx::VectorIconId::MIXED_CONTENT,
     IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
     IDR_BLOCKED_PPAPI_BROKER,
     gfx::VectorIconId::EXTENSION,
     IDS_BLOCKED_PPAPI_BROKER_TITLE,
     0,
     IDR_BLOCKED_PPAPI_BROKER,
     IDS_ALLOWED_PPAPI_BROKER_TITLE},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
     IDR_BLOCKED_DOWNLOADS,
     gfx::VectorIconId::FILE_DOWNLOAD,
     IDS_BLOCKED_DOWNLOAD_TITLE,
     IDS_BLOCKED_DOWNLOADS_EXPLANATION,
     IDR_ALLOWED_DOWNLOADS,
     IDS_ALLOWED_DOWNLOAD_TITLE},
};

const ContentSettingsImageDetails* GetImageDetails(ContentSettingsType type) {
  for (const ContentSettingsImageDetails& image_details : kImageDetails) {
    if (image_details.type == type)
      return &image_details;
  }
  return nullptr;
}

}  // namespace

// Single content setting ------------------------------------------------------

ContentSettingSimpleImageModel::ContentSettingSimpleImageModel(
    ContentSettingsType content_type)
    : ContentSettingImageModel(),
      content_type_(content_type) {
}

ContentSettingBubbleModel* ContentSettingSimpleImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return ContentSettingBubbleModel::CreateContentSettingBubbleModel(
      delegate,
      web_contents,
      profile,
      content_type());
}

bool ContentSettingSimpleImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  if (!web_contents)
    return false;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;

  return !content_settings->IsBlockageIndicated(content_type());
}

void ContentSettingSimpleImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (content_settings)
    content_settings->SetBlockageHasBeenIndicated(content_type());
}

// static
std::unique_ptr<ContentSettingImageModel>
ContentSettingSimpleImageModel::CreateForContentTypeForTesting(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == CONTENT_SETTINGS_TYPE_GEOLOCATION)
    return base::WrapUnique(new ContentSettingGeolocationImageModel());

  if (content_settings_type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS)
    return base::WrapUnique(new ContentSettingRPHImageModel());

  if (content_settings_type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX)
    return base::WrapUnique(new ContentSettingMIDISysExImageModel());

  return base::WrapUnique(
      new ContentSettingBlockedImageModel(content_settings_type));
}

// Generic blocked content settings --------------------------------------------

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ContentSettingsType content_type)
    : ContentSettingSimpleImageModel(content_type) {
}

void ContentSettingBlockedImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  const ContentSettingsType type = content_type();
  const ContentSettingsImageDetails* image_details = GetImageDetails(type);
  DCHECK(image_details) << "No entry for " << type << " in kImageDetails[].";

  int icon_id = image_details->blocked_icon_id;
  int tooltip_id = image_details->blocked_tooltip_id;
  int explanation_id = image_details->blocked_explanatory_text_id;

  // For plugins, don't show the animated explanation unless the plugin was
  // blocked despite the user's content settings being set to allow it (e.g.
  // due to auto-blocking NPAPI plugins).
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    GURL url = web_contents->GetURL();
    if (map->GetContentSetting(url, url, type, std::string()) !=
        CONTENT_SETTING_ALLOW)
      explanation_id = 0;
  }

  // If a content type is blocked by default and was accessed, display the
  // content blocked page action.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (!content_settings->IsContentBlocked(type)) {
    if (!content_settings->IsContentAllowed(type))
      return;

    // For cookies, only show the cookie blocked page action if cookies are
    // blocked by default.
    if (type == CONTENT_SETTINGS_TYPE_COOKIES &&
        (map->GetDefaultContentSetting(type, NULL) != CONTENT_SETTING_BLOCK))
      return;

    icon_id = image_details->accessed_icon_id;
    tooltip_id = image_details->accessed_tooltip_id;
    explanation_id = 0;
  }
  set_visible(true);
  if (!UseVectorGraphics()) {
    DCHECK(icon_id);
    SetIconByResourceId(icon_id);
  } else {
    gfx::VectorIconId badge_id = gfx::VectorIconId::VECTOR_ICON_NONE;
    if (type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER)
      badge_id = gfx::VectorIconId::WARNING_BADGE;
    else if (content_settings->IsContentBlocked(type))
      badge_id = gfx::VectorIconId::BLOCKED_BADGE;

    set_icon_by_vector_id(image_details->vector_icon_id, badge_id);
  }
  set_explanatory_string_id(explanation_id);
  DCHECK(tooltip_id);
  set_tooltip(l10n_util::GetStringUTF16(tooltip_id));
}

// Geolocation -----------------------------------------------------------------

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingSimpleImageModel(CONTENT_SETTINGS_TYPE_GEOLOCATION) {
}

void ContentSettingGeolocationImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const ContentSettingsUsagesState& usages_state = content_settings->
      geolocation_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(NULL, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  if (!UseVectorGraphics()) {
    SetIconByResourceId(allowed ? IDR_ALLOWED_LOCATION : IDR_BLOCKED_LOCATION);
  } else {
    set_icon_by_vector_id(gfx::VectorIconId::MY_LOCATION,
                          allowed ? gfx::VectorIconId::VECTOR_ICON_NONE
                                  : gfx::VectorIconId::BLOCKED_BADGE);
  }
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_GEOLOCATION_ALLOWED_TOOLTIP
                                            : IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

// Media -----------------------------------------------------------------------

ContentSettingMediaImageModel::ContentSettingMediaImageModel()
    : ContentSettingImageModel() {
}

void ContentSettingMediaImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);

  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  TabSpecificContentSettings::MicrophoneCameraState state =
      content_settings->GetMicrophoneCameraState();

  // If neither the microphone nor the camera stream was accessed then no icon
  // is displayed in the omnibox.
  if (state == TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED)
    return;

  bool is_mic = (state & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
  bool is_cam = (state & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
  DCHECK(is_mic || is_cam);

  int id = IDS_CAMERA_BLOCKED;
  if (state & (TabSpecificContentSettings::MICROPHONE_BLOCKED |
               TabSpecificContentSettings::CAMERA_BLOCKED)) {
    if (!UseVectorGraphics()) {
      SetIconByResourceId(IDR_BLOCKED_CAMERA);
    } else {
      set_icon_by_vector_id(gfx::VectorIconId::VIDEOCAM,
                            gfx::VectorIconId::BLOCKED_BADGE);
    }
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_BLOCKED : IDS_MICROPHONE_BLOCKED;
  } else {
    if (!UseVectorGraphics()) {
      SetIconByResourceId(IDR_ALLOWED_CAMERA);
    } else {
      set_icon_by_vector_id(gfx::VectorIconId::VIDEOCAM,
                            gfx::VectorIconId::VECTOR_ICON_NONE);
    }
    id = IDS_CAMERA_ACCESSED;
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_ALLOWED : IDS_MICROPHONE_ACCESSED;
  }
  set_tooltip(l10n_util::GetStringUTF16(id));
  set_visible(true);
}

ContentSettingBubbleModel* ContentSettingMediaImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return new ContentSettingMediaStreamBubbleModel(delegate,
                                                  web_contents,
                                                  profile);
}

bool ContentSettingMediaImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  if (!web_contents)
    return false;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  return (!content_settings->IsBlockageIndicated(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) &&
          !content_settings->IsBlockageIndicated(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
}

void ContentSettingMediaImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (content_settings) {
    content_settings->SetBlockageHasBeenIndicated(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
    content_settings->SetBlockageHasBeenIndicated(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }
}

// Subresource Filter ----------------------------------------------------------

ContentSettingSubresourceFilterImageModel::
    ContentSettingSubresourceFilterImageModel()
    : ContentSettingImageModel() {}

void ContentSettingSubresourceFilterImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);

  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

  if (!content_settings || !content_settings->IsSubresourceBlocked())
    return;

  set_icon_by_vector_id(gfx::VectorIconId::SUBRESOURCE_FILTER_ACTIVE,
                        gfx::VectorIconId::BLOCKED_BADGE);
  set_explanatory_string_id(IDS_FILTERED_DECEPTIVE_CONTENT_PROMPT_TITLE);
  // TODO(melandory): Set tooltip text.
  set_visible(true);
}

ContentSettingBubbleModel*
ContentSettingSubresourceFilterImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return new ContentSettingSubresourceFilterBubbleModel(delegate, web_contents,
                                                        profile);
}

bool ContentSettingSubresourceFilterImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  if (!web_contents)
    return false;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  return content_settings && content_settings->IsSubresourceBlockageIndicated();
}

void ContentSettingSubresourceFilterImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (content_settings)
    content_settings->SetSubresourceBlockageIndicated();
}

// Protocol handlers -----------------------------------------------------------

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingSimpleImageModel(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
  if (!UseVectorGraphics()) {
    SetIconByResourceId(IDR_REGISTER_PROTOCOL_HANDLER);
  } else {
    set_icon_by_vector_id(gfx::VectorIconId::PROTOCOL_HANDLER,
                          gfx::VectorIconId::VECTOR_ICON_NONE);
  }
  set_tooltip(l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_TOOLTIP));
}

void ContentSettingRPHImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (content_settings->pending_protocol_handler().IsEmpty())
    return;

  set_visible(true);
}

// MIDI SysEx ------------------------------------------------------------------

ContentSettingMIDISysExImageModel::ContentSettingMIDISysExImageModel()
    : ContentSettingSimpleImageModel(CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
}

void ContentSettingMIDISysExImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(NULL, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  if (!UseVectorGraphics()) {
    SetIconByResourceId(allowed ? IDR_ALLOWED_MIDI_SYSEX
                                : IDR_BLOCKED_MIDI_SYSEX);
  } else {
    set_icon_by_vector_id(gfx::VectorIconId::MIDI,
                          allowed ? gfx::VectorIconId::VECTOR_ICON_NONE
                                  : gfx::VectorIconId::BLOCKED_BADGE);
  }
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_MIDI_SYSEX_ALLOWED_TOOLTIP
                                            : IDS_MIDI_SYSEX_BLOCKED_TOOLTIP));
}

// Base class ------------------------------------------------------------------

gfx::Image ContentSettingImageModel::GetIcon(SkColor nearby_text_color) const {
  SkColor icon_color = nearby_text_color;
#if !defined(OS_MACOSX)
  icon_color = color_utils::DeriveDefaultIconColor(nearby_text_color);
#endif

  return UseVectorGraphics()
             ? gfx::Image(gfx::CreateVectorIconWithBadge(
                   vector_icon_id_, 16, icon_color, vector_icon_badge_id_))
             : raster_icon_;
}

ContentSettingImageModel::ContentSettingImageModel()
    : is_visible_(false),
      raster_icon_id_(0),
      vector_icon_id_(gfx::VectorIconId::VECTOR_ICON_NONE),
      vector_icon_badge_id_(gfx::VectorIconId::VECTOR_ICON_NONE),
      explanatory_string_id_(0) {}

// static
ScopedVector<ContentSettingImageModel>
    ContentSettingImageModel::GenerateContentSettingImageModels() {
  ScopedVector<ContentSettingImageModel> result;

  // The ordering of the models here influences the order in which icons are
  // shown in the omnibox.
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_COOKIES));
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_IMAGES));
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_PPAPI_BROKER));
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_PLUGINS));
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_POPUPS));
  result.push_back(new ContentSettingGeolocationImageModel());
  result.push_back(
      new ContentSettingBlockedImageModel(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
  result.push_back(new ContentSettingRPHImageModel());
  result.push_back(new ContentSettingMediaImageModel());
  result.push_back(new ContentSettingSubresourceFilterImageModel());
  result.push_back(
      new ContentSettingBlockedImageModel(
          CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS));
  result.push_back(new ContentSettingMIDISysExImageModel());

  return result;
}

void ContentSettingImageModel::SetIconByResourceId(int id) {
  raster_icon_id_ = id;
  raster_icon_ =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(id);
}

#if defined(OS_MACOSX)
bool ContentSettingImageModel::UpdateFromWebContentsAndCheckIfIconChanged(
    content::WebContents* web_contents) {
  if (UseVectorGraphics()) {
    gfx::VectorIconId old_vector_icon = vector_icon_id_;
    gfx::VectorIconId old_badge_icon = vector_icon_badge_id_;
    UpdateFromWebContents(web_contents);
    return old_vector_icon != vector_icon_id_ &&
           old_badge_icon != vector_icon_badge_id_;
  } else {
    int old_icon = raster_icon_id_;
    UpdateFromWebContents(web_contents);
    return old_icon != raster_icon_id_;
  }
}
#endif
