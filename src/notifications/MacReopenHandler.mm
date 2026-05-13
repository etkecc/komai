// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#import "notifications/MacReopenHandler.h"

#import <AppKit/NSApplication.h>
#import <Foundation/Foundation.h>

@interface KomaiReopenHandler : NSObject <NSApplicationDelegate> {
    std::function<void()> _onReopen;
}
@property (nonatomic, strong) id<NSApplicationDelegate> nextDelegate;
- (instancetype)initWithCallback:(std::function<void()>)onReopen
                    nextDelegate:(id<NSApplicationDelegate>)next;
@end

@implementation KomaiReopenHandler

- (instancetype)initWithCallback:(std::function<void()>)onReopen
                    nextDelegate:(id<NSApplicationDelegate>)next
{
    if ((self = [super init])) {
        _onReopen = std::move(onReopen);
        _nextDelegate = next;
    }
    return self;
}

// AppKit calls this on the main thread whenever the user clicks the dock icon on a running app; flag is NO when there are no visible windows, which is precisely the case komai needs to react to ("start in tray", or close-to-tray when QApplication::applicationStateChanged didn't fire because the app never went Inactive first).
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if (!flag && _onReopen) {
        _onReopen();
    }
    if ([_nextDelegate respondsToSelector:_cmd]) {
        return [_nextDelegate applicationShouldHandleReopen:sender hasVisibleWindows:flag];
    }
    return YES;
}

// Forward every other delegate selector to whatever delegate Qt installed, so we don't accidentally swallow notifications, URL-open events, or termination requests by stealing the slot.
- (BOOL)respondsToSelector:(SEL)aSelector
{
    if ([super respondsToSelector:aSelector])
        return YES;
    return [_nextDelegate respondsToSelector:aSelector];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    if ([_nextDelegate respondsToSelector:aSelector])
        return _nextDelegate;
    return nil;
}

@end

namespace komai::mac {

void
installReopenHandler(std::function<void()> onReopen)
{
    static KomaiReopenHandler *handler = nil;
    if (handler != nil)
        return;

    handler = [[KomaiReopenHandler alloc] initWithCallback:std::move(onReopen)
                                              nextDelegate:NSApp.delegate];
    NSApp.delegate = handler;
}

} // namespace komai::mac
