// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension_id.h"

class BackgroundModeOptimizer;
class BackgroundTrigger;
class Browser;
class PrefRegistrySimple;
class Profile;
class StatusIcon;
class StatusTray;

namespace base {
class CommandLine;
}

namespace extensions {
class Extension;
}

typedef std::vector<base::Closure> CommandIdHandlerVector;

// BackgroundModeManager is responsible for switching Chrome into and out of
// "background mode" and for providing UI for the user to exit Chrome when there
// are no open browser windows.
//
// Chrome enters background mode whenever there is an application with the
// "background" permission installed, or a "trigger" that requires Chrome to be
// running. This class monitors the set of installed/loaded extensions to ensure
// that Chrome enters/exits background mode at the appropriate time.
//
// When Chrome is in background mode, it will continue running even after the
// last browser window is closed, until the user explicitly exits the app.
// Additionally, when in background mode, Chrome will launch on OS login with
// no open windows to allow apps with the "background" permission to run in the
// background.
class BackgroundModeManager
    : public content::NotificationObserver,
      public chrome::BrowserListObserver,
      public BackgroundApplicationListModel::Observer,
      public ProfileAttributesStorage::Observer,
      public StatusIconMenuModel::Delegate {
 public:
  BackgroundModeManager(const base::CommandLine& command_line,
                        ProfileAttributesStorage* profile_storage);
  ~BackgroundModeManager() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual void RegisterProfile(Profile* profile);

  static void LaunchBackgroundApplication(Profile* profile,
      const extensions::Extension* extension);

  // Gets a browser window for |profile| associated with the active desktop.
  // Opens a new browser window if there isn't one for the active desktop.
  static Browser* GetBrowserWindowForProfile(Profile* profile);

  // Getter and setter for the flag indicating whether Chrome should start in
  // background mode the next time.
  static bool should_restart_in_background() {
    return should_restart_in_background_;
  }

  static void set_should_restart_in_background(bool enable) {
    should_restart_in_background_ = enable;
  }

  // Returns true if background mode is active.
  virtual bool IsBackgroundModeActive();

  // Suspends background mode until either ResumeBackgroundMode is called or
  // Chrome is restarted. This has the same effect as ending background mode
  // for the current browser session.
  virtual void SuspendBackgroundMode();

  // Resumes background mode. This ends a suspension of background mode, but
  // will not start it if it is not enabled.
  virtual void ResumeBackgroundMode();

  // For testing purposes.
  int NumberOfBackgroundModeData();

  // Registers |trigger| as a reason for enabling background mode. Does not
  // take ownership of |profile| or |trigger|.
  void RegisterTrigger(Profile* profile,
                       BackgroundTrigger* trigger,
                       bool should_notify_user);

  // Unregisters |trigger| as a reason for enabling background mode.
  void UnregisterTrigger(Profile* profile, BackgroundTrigger* trigger);

 private:
  friend class AppBackgroundPageApiTest;
  friend class BackgroundModeManagerTest;
  friend class BackgroundModeManagerWithExtensionsTest;
  friend class AdvancedTestBackgroundModeManager;
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppLoadUnload);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundLaunchOnStartup);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundModeDisabledPreventsKeepAliveOnStartup);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           DisableBackgroundModeUnderTestFlag);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           EnableAfterBackgroundAppInstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           MultiProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ProfileAttributesStorage);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ProfileAttributesStorageObserver);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           DeleteBackgroundProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           TriggerRegisterUnregister);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           TriggerRegisterWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BackgroundMenuGeneration);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BackgroundMenuGenerationMultipleProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BalloonDisplay);
  FRIEND_TEST_ALL_PREFIXES(BackgroundAppBrowserTest,
                           ReloadBackgroundApp);

  // A pending trigger may be registered later. The boolean indicates whether
  // the user should be notified when it is registered.
  using PendingTriggerData = std::map<BackgroundTrigger*, bool>;

  // Manages the background clients and menu items for a single profile. A
  // client can be a trigger or an extension.
  class BackgroundModeData : public StatusIconMenuModel::Delegate {
   public:
    BackgroundModeData(Profile* profile,
                       CommandIdHandlerVector* command_id_handler_vector);
    ~BackgroundModeData() override;

    // The cached list of BackgroundApplications.
    std::unique_ptr<BackgroundApplicationListModel> applications_;

    // Overrides from StatusIconMenuModel::Delegate implementation.
    void ExecuteCommand(int command_id, int event_flags) override;

    // Returns a browser window, or creates one if none are open. Used by
    // operations (like displaying the preferences dialog) that require a
    // Browser window.
    Browser* GetBrowserWindow();

    // Returns the number of background clients for this profile. A client can
    // be a trigger or an extension.
    int GetBackgroundClientCount() const;

    // Builds the profile specific parts of the menu. The menu passed in may
    // be a submenu in the case of multi-profiles or the main menu in the case
    // of the single profile case. If containing_menu is valid, we will add
    // menu as a submenu to it.
    void BuildProfileMenu(StatusIconMenuModel* menu,
                          StatusIconMenuModel* containing_menu);

    // Set the name associated with this background mode data for displaying in
    // the status tray.
    void SetName(const base::string16& new_profile_name);

    // The name associated with this background mode data. This should match
    // the name in the ProfileAttributesStorage for this profile.
    base::string16 name();

    // Used for sorting BackgroundModeData*s.
    static bool BackgroundModeDataCompare(const BackgroundModeData* bmd1,
                                          const BackgroundModeData* bmd2);

    // Returns the set of new background apps (apps that have been loaded since
    // the last call to GetNewBackgroundApps()).
    std::set<const extensions::Extension*> GetNewBackgroundApps();

    // Adds a pending |trigger| as a reason to enable background mode. A pending
    // trigger does not activate background mode, but may be registered later by
    // RegisterPendingTriggers. Does not take ownership of |trigger|.
    void AddPendingTrigger(BackgroundTrigger* trigger, bool should_notify_user);

    // The pending trigger data.
    PendingTriggerData GetPendingTriggerData() const;

    // Clears the pending trigger data.
    void ClearPendingTriggerData();

    // Counts the number of pending triggers.
    int GetPendingTriggerCount() const;

    // Registers |trigger| as a reason to enable background mode. Does not take
    // ownership of |trigger|. Idempotent.
    void RegisterTrigger(BackgroundTrigger* trigger);

    // Removes |trigger| from the registered and pending triggers. Idempotent.
    void UnregisterTrigger(BackgroundTrigger* trigger);

    // Checks whether |trigger| is already registered or stored as a pending
    // trigger.
    bool HasTrigger(BackgroundTrigger* trigger);

   private:
    // Name associated with this profile which is used to label its submenu.
    base::string16 name_;

    // The profile associated with this background app data.
    Profile* profile_;

    // Weak ref vector owned by BackgroundModeManager where the indices
    // correspond to Command IDs and values correspond to their handlers.
    CommandIdHandlerVector* command_id_handler_vector_;

    // The list of notified extensions for this profile. We track this to ensure
    // that we never notify the user about the same extension twice in a single
    // browsing session - this is done because the extension subsystem is not
    // good about tracking changes to the background permission around
    // extension reloads, and will sometimes report spurious permission changes.
    std::set<extensions::ExtensionId> current_extensions_;

    // This class does not own the triggers. If a trigger does not outlive this
    // class it must be unregistered before destruction.
    PendingTriggerData pending_trigger_data_;

    // This class does not own the triggers. If a trigger does not outlive this
    // class it must be unregistered before destruction.
    std::set<BackgroundTrigger*> registered_triggers_;
  };

  // Ideally we would want our BackgroundModeData to be scoped_ptrs,
  // but since maps copy their entries, we can't used scoped_ptrs.
  // Similarly, we can't just have a map of BackgroundModeData objects,
  // since BackgroundModeData contains a scoped_ptr which once again
  // can't be copied. So rather than using BackgroundModeData* which
  // we'd have to remember to delete, we use the ref-counted linked_ptr
  // which is similar to a shared_ptr.
  typedef linked_ptr<BackgroundModeData> BackgroundModeInfo;

  typedef std::map<Profile*, BackgroundModeInfo> BackgroundModeInfoMap;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when ExtensionSystem is ready.
  void OnExtensionsReady(Profile* profile);

  // Called when the kBackgroundModeEnabled preference changes.
  void OnBackgroundModeEnabledPrefChanged();

  // BackgroundApplicationListModel::Observer implementation.
  void OnApplicationDataChanged(const extensions::Extension* extension,
                                Profile* profile) override;
  void OnApplicationListChanged(Profile* profile) override;

  // Overrides from ProfileAttributesStorage::Observer
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;

  // Overrides from StatusIconMenuModel::Delegate implementation.
  void ExecuteCommand(int command_id, int event_flags) override;

  // chrome::BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

  // Enables or disables background mode as needed, taking into account the
  // number of background clients. Updates the background status of |profile| in
  // the ProfileAttributesStorage if needed. If |new_client_names| is not empty
  // the user will be notified about the added client(s).
  void OnClientsChanged(Profile* profile,
                        const std::vector<base::string16>& new_client_names);

  // Invoked when a background client is installed so we can ensure that
  // launch-on-startup is enabled if appropriate.
  void OnBackgroundClientInstalled(const base::string16& name);

  // Walk the list of profiles and see if an extension or app is being
  // currently upgraded or reloaded by any profile.  If so, update the
  // output variables appropriately.
  void CheckReloadStatus(
      const extensions::Extension* extension,
      bool* is_being_reloaded);

  // Called to make sure that our launch-on-startup mode is properly set.
  // (virtual so it can be mocked in tests).
  virtual void EnableLaunchOnStartup(bool should_launch);

  // Invoked when a client is installed so we can display a platform-specific
  // notification.
  virtual void DisplayClientInstalledNotification(const base::string16& name);

  // Invoked to put Chrome in KeepAlive mode - chrome runs in the background
  // and has a status bar icon.
  void StartBackgroundMode();

  // Invoked to take Chrome out of KeepAlive mode - Chrome stops running in
  // the background and removes its status bar icon.
  void EndBackgroundMode();

  // Enables keep alive and the status tray icon if and only if background mode
  // is active and not suspended.
  virtual void UpdateKeepAliveAndTrayIcon();

  // Release keep_alive_for_startup_. This is invoked as a callback to make
  // make sure the message queue was initialized before we attempt to exit.
  void ReleaseStartupKeepAliveCallback();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for apps to load. This is called when we no
  // longer need to do this (either because the user has chosen to exit chrome
  // manually, or all apps have been loaded).
  void ReleaseStartupKeepAlive();

  // Return an appropriate name for a Preferences menu entry.  Preferences is
  // sometimes called Options or Settings.
  base::string16 GetPreferencesMenuLabel();

  // Create a status tray icon to allow the user to shutdown Chrome when running
  // in background mode. Virtual to enable testing.
  virtual void CreateStatusTrayIcon();

  // Removes the status tray icon because we are exiting background mode.
  // Virtual to enable testing.
  virtual void RemoveStatusTrayIcon();

  // Create a context menu, or replace/update an existing context menu, for the
  // status tray icon which, among other things, allows the user to shutdown
  // Chrome when running in background mode. All profiles are listed under
  // the one context menu.
  virtual void UpdateStatusTrayIconContextMenu();

  // Returns the BackgroundModeData associated with this profile. If it does
  // not exist, returns NULL.
  BackgroundModeData* GetBackgroundModeData(Profile* const profile) const;

  // Returns the iterator associated with a particular profile name.
  // This should not be used to iterate over the background mode data. It is
  // used to efficiently delete an item from the background mode data map.
  BackgroundModeInfoMap::iterator GetBackgroundModeIterator(
      const base::string16& profile_name);

  // Returns true if the "Let chrome run in the background" pref is checked.
  // (virtual to allow overriding in tests).
  virtual bool IsBackgroundModePrefEnabled() const;

  // Turns off background mode if it's currently enabled.
  void DisableBackgroundMode();

  // Turns on background mode if it's currently disabled.
  void EnableBackgroundMode();

  // Returns the number of background clients in the system, for all profiles.
  // A client can be a trigger or an extension.
  // (virtual to allow overriding in unit tests)
  virtual int GetBackgroundClientCount() const;

  // Returns the number of background clients for a profile. A client can be a
  // trigger or an extension.
  virtual int GetBackgroundClientCountForProfile(Profile* const profile) const;

  // Returns the number of pending triggers in the system, for all profiles.
  int GetPendingTriggerCount() const;

  // Returns true if we should be in background mode.
  bool ShouldBeInBackgroundMode() const;

  // Finds the BackgroundModeData associated with the last active profile,
  // if the profile isn't locked. Returns NULL otherwise.
  BackgroundModeData* GetBackgroundModeDataForLastProfile() const;

  // Set to true when the next restart should be done in background mode.
  // Static because its value is read after the background mode manager is
  // destroyed.
  static bool should_restart_in_background_;

  // Reference to the ProfileAttributesStorage. It is used to update the
  // background app status of profiles when they open/close background apps.
  ProfileAttributesStorage* profile_storage_;

  // Registrars for managing our change observers.
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_registrar_;

  // The profile-keyed data for this background mode manager. Keyed on profile.
  BackgroundModeInfoMap background_mode_data_;

  // Indexes the command ids for the entire background menu to their handlers.
  CommandIdHandlerVector command_id_handler_vector_;

  // Maintains submenu lifetime for the multiple profile context menu.
  ScopedVector<StatusIconMenuModel> submenus;

  // Reference to our status tray. If null, the platform doesn't support status
  // icons.
  StatusTray* status_tray_;

  // Reference to our status icon (if any) - owned by the StatusTray.
  StatusIcon* status_icon_;

  // Reference to our status icon's context menu (if any) - owned by the
  // status_icon_.
  StatusIconMenuModel* context_menu_;

  // Set to true when we are running in background mode. Allows us to track our
  // current background state so we can take the appropriate action when the
  // user disables/enables background mode via preferences.
  bool in_background_mode_;

  // Background mode does not always keep Chrome alive. When it does, it is
  // using this scoped object.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Set when we are keeping chrome running during the startup process - this
  // is required when running with the --no-startup-window flag, as otherwise
  // chrome would immediately exit due to having no open windows.
  std::unique_ptr<ScopedKeepAlive> keep_alive_for_startup_;

  // Reference to the optimizer to use to reduce Chrome's footprint when in
  // background mode. If null, optimizations are disabled.
  std::unique_ptr<BackgroundModeOptimizer> optimizer_;

  // Set to true when Chrome is running with the --keep-alive-for-test flag
  // (used for testing background mode without having to install a background
  // app).
  bool keep_alive_for_test_;

  // Set to true when background mode is suspended.
  bool background_mode_suspended_;

  base::WeakPtrFactory<BackgroundModeManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
