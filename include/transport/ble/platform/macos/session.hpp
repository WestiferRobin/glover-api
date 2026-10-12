#ifndef GLOVER_BLE_MACOS_SESSION_HPP
#define GLOVER_BLE_MACOS_SESSION_HPP
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <chrono>
#include <exception>

// Private main-run-loop deadline helper, not a transport abstraction.
@interface GBBleSession : NSObject {
@public
    // First C++ callback failure; exception_ptr recording does not allocate.
    std::exception_ptr cppFailure;
}
@property(nonatomic) BOOL finished;
@property(nonatomic, copy) NSString *error;
@property(nonatomic, copy) NSString *stage;
@property(nonatomic) NSTimeInterval seconds;
@property(nonatomic, strong) NSTimer *timer;
- (void)watch:(NSString *)stage timeout:(void (^)(void))handler;
- (void)finish;
@end

void GBLog(NSString *message);
NSString *GBState(CBManagerState state);
NSString *GBAuthorization();
NSString *GBManagerProblem(CBManagerState state);
void GBValidateRun(std::chrono::milliseconds timeout);
void GBWait(GBBleSession *session);
void GBDrainPeripheral(CBPeripheralManager *manager, GBBleSession *original);
void GBThrowIfFailed(GBBleSession *session);
#endif
