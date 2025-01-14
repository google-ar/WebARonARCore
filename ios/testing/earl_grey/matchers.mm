// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace testing {

id<GREYMatcher> contextMenuItemWithText(NSString* text) {
  // Both tablet and phone house context menu views inside an alert controller
  // view (on tablet that view is itself inside a popover view).
  id<GREYMatcher> context_menu_container =
      grey_kindOfClass(NSClassFromString(@"_UIAlertControllerView"));

  return grey_allOf(grey_ancestor(context_menu_container), grey_interactable(),
                    grey_text(text), nil);
}

id<GREYMatcher> elementToDismissContextMenu(NSString* cancel_text) {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom == UIUserInterfaceIdiomPad) {
    // On iPad the context menu is dismissed by tapping on something
    // that isn't the popover. UIKit conveniently labels this element.
    return grey_accessibilityID(@"PopoverDismissRegion");
  } else {
    // On iPhone the context menu is dismissed by tapping on the "Cancel" item.
    return contextMenuItemWithText(cancel_text);
  }
}

}  // namespace testing
