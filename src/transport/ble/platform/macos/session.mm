#import <transport/ble/platform/macos/session.hpp>
#include <cstdio>
#include <stdexcept>

@implementation GBBleSession
- (void)watch:(NSString *)stage timeout:(void (^)(void))handler {
    if (self.finished) return;
    [self.timer invalidate];
    self.stage = stage;
    GBLog([@"stage: " stringByAppendingString:stage]);
    self.timer = [NSTimer timerWithTimeInterval:self.seconds repeats:NO block:^(NSTimer *) {
        handler();
    }];
    [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSDefaultRunLoopMode];
}
- (void)finish {
    if (self.finished) return;
    [self.timer invalidate];
    self.timer = nil;
    self.finished = YES;
    // Timer delivery alone need not return NSRunLoop's input-source wait.
    CFRunLoopStop(CFRunLoopGetMain());
}
- (void)dealloc { [_timer invalidate]; }
@end

void GBLog(NSString *message) {
    std::fprintf(stderr, "%s\n", message.UTF8String ? message.UTF8String : "BLE diagnostic unavailable");
    std::fflush(stderr);
}
NSString *GBState(CBManagerState state) {
    switch (state) {
        case CBManagerStateUnknown: return @"unknown";
        case CBManagerStateResetting: return @"resetting";
        case CBManagerStateUnsupported: return @"unsupported";
        case CBManagerStateUnauthorized: return @"unauthorized";
        case CBManagerStatePoweredOff: return @"poweredOff";
        case CBManagerStatePoweredOn: return @"poweredOn";
    }
    return @"unrecognized";
}
NSString *GBAuthorization() {
    switch (CBManager.authorization) {
        case CBManagerAuthorizationNotDetermined: return @"notDetermined";
        case CBManagerAuthorizationRestricted: return @"restricted";
        case CBManagerAuthorizationDenied: return @"denied";
        case CBManagerAuthorizationAllowedAlways: return @"allowedAlways";
    }
    return @"unrecognized";
}
NSString *GBManagerProblem(CBManagerState state) {
    if (CBManager.authorization == CBManagerAuthorizationDenied ||
        CBManager.authorization == CBManagerAuthorizationRestricted ||
        state == CBManagerStateUnauthorized) return @"Bluetooth permission denied or restricted";
    if (state == CBManagerStateUnsupported) return @"Bluetooth unavailable: unsupported manager";
    if (state == CBManagerStatePoweredOff) return @"Bluetooth unavailable: powered off";
    return nil;
}
void GBValidateRun(std::chrono::milliseconds timeout) {
    if (![NSThread isMainThread]) throw std::logic_error("BLE experiment must run on the main thread");
    if (timeout.count() < 1 || timeout.count() > 60000) {
        throw std::invalid_argument("BLE stage timeout must be 1..60000 ms");
    }
}
void GBWait(GBBleSession *session) {
    while (!session.finished) {
        @autoreleasepool {
            // Sleeps in the native run loop until callbacks or the stage timer fire.
            [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
        }
    }
}
void GBThrowIfFailed(GBBleSession *session) {
    // Called only by the synchronous C++ entry point, after platform cleanup.
    if (session->cppFailure) std::rethrow_exception(session->cppFailure);
    if (session.error) throw std::runtime_error(session.error.UTF8String ? session.error.UTF8String : "BLE experiment failed");
}

void GBDrainPeripheral(CBPeripheralManager *manager, GBBleSession *original) {
    GBBleSession *drain = [GBBleSession new];
    // Native timers keep teardown finite and allow Bluetooth state updates to run.
    drain.seconds = 0.25;
    __weak GBBleSession *weakDrain = drain;
    [drain watch:@"peripheral disconnect grace" timeout:^{ [weakDrain finish]; }];
    GBWait(drain);
    [manager stopAdvertising];
    GBBleSession *stop = [GBBleSession new];
    stop.seconds = 1.0;
    __weak GBBleSession *weakStop = stop;
    [stop watch:@"advertising shutdown" timeout:^{
        if (manager.isAdvertising) {
            NSString *message = @"Advertising shutdown not confirmed within 1000 ms";
            original.error = original.error ? [original.error stringByAppendingFormat:@"; %@", message] : message;
        }
        [weakStop finish];
    }];
    NSTimer *observer = [NSTimer scheduledTimerWithTimeInterval:0.05 repeats:YES block:^(NSTimer *) {
        if (!manager.isAdvertising) [weakStop finish];
    }];
    @try { GBWait(stop); } @finally { [observer invalidate]; [stop finish]; }
    GBLog(manager.isAdvertising ? @"peripheral: local advertising shutdown unconfirmed" : @"peripheral: isAdvertising=NO (local state, not radio proof)");
}
