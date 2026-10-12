#include <transport/ble/peripheral.hpp>
#import <transport/ble/platform/macos/session.hpp>

typedef NS_ENUM(NSInteger, GBPeripheralPhase) {
    GBPInitializing, GBPPublishing, GBPAdvertising, GBPPeer, GBPQueue, GBPUnsubscribe
};
@interface GBBlePeripheralDelegate : NSObject <CBPeripheralManagerDelegate> {
@public
    BleConfig config;
    std::vector<std::string> messages;
    std::size_t nextMessage;
}
@property(nonatomic, strong) GBBleSession *session;
@property(nonatomic, strong) CBPeripheralManager *manager;
@property(nonatomic, strong) CBMutableService *service;
@property(nonatomic, strong) CBMutableCharacteristic *characteristic;
@property(nonatomic, strong) CBCentral *peer;
@property(nonatomic) GBPeripheralPhase phase;
@property(nonatomic) BOOL readServed;
@property(nonatomic) BOOL subscribed;
@property(nonatomic) BOOL notificationQueued;
- (void)watch:(GBPeripheralPhase)phase label:(NSString *)label;
- (void)fail:(NSString *)message;
- (void)sendNotification;
- (void)cleanup;
@end

@implementation GBBlePeripheralDelegate
- (void)watch:(GBPeripheralPhase)phase label:(NSString *)label {
    self.phase = phase;
    __weak GBBlePeripheralDelegate *weakSelf = self;
    [self.session watch:label timeout:^{
        GBBlePeripheralDelegate *owner = weakSelf;
        if (owner && !owner.session.finished) [owner fail:[@"Timeout: " stringByAppendingString:label]];
    }];
}
- (void)fail:(NSString *)message {
    if (self.session.finished) return;
    self.session.error = message;
    [self.session finish];
}
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
    if (self.session.finished) return;
    GBLog([NSString stringWithFormat:@"peripheral callback: state=%@ authorization=%@",
           GBState(peripheral.state), GBAuthorization()]);
    NSString *problem = GBManagerProblem(peripheral.state);
    if (problem) { [self fail:problem]; return; }
    if (peripheral.state != CBManagerStatePoweredOn) {
        if (self.phase != GBPInitializing) [self fail:@"Bluetooth became unavailable during peripheral session"];
        return;
    }
    if (self.phase != GBPInitializing) return;
    // A nil value makes reads dynamic and allows read+notify on the same characteristic.
    self.characteristic = [[CBMutableCharacteristic alloc] initWithType:[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.characteristic.c_str()]]
        properties:CBCharacteristicPropertyRead | CBCharacteristicPropertyNotify
        value:nil permissions:CBAttributePermissionsReadable];
    self.service = [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.service.c_str()]] primary:YES];
    self.service.characteristics = @[self.characteristic];
    [self watch:GBPPublishing label:@"service publication"];
    [peripheral addService:self.service];
}
- (void)peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error {
    if (self.session.finished || self.phase != GBPPublishing || service != self.service) return;
    if (error) { [self fail:[@"Service publication failure: " stringByAppendingString:error.description]]; return; }
    GBLog(@"peripheral callback: service published");
    [self watch:GBPAdvertising label:@"advertising start"];
    [peripheral startAdvertising:@{CBAdvertisementDataServiceUUIDsKey: @[[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.service.c_str()]]]}];
}
- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    if (self.session.finished || self.phase != GBPAdvertising) return;
    if (error || !peripheral.isAdvertising) {
        [self fail:[NSString stringWithFormat:@"Advertising failure: %@", error ? error.description : @"manager is not advertising"]]; return;
    }
    GBLog(@"peripheral callback: advertising started (not independent discovery evidence)");
    [self watch:GBPPeer label:@"peer read/subscription"];
}
- (void)peripheralManager:(CBPeripheralManager *)peripheral didReceiveReadRequest:(CBATTRequest *)request {
    if (self.session.finished) return;
    if (![request.characteristic.UUID isEqual:[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.characteristic.c_str()]]]) {
        [peripheral respondToRequest:request withResult:CBATTErrorAttributeNotFound]; return;
    }
    if (self.peer && ![self.peer.identifier isEqual:request.central.identifier]) {
        [peripheral respondToRequest:request withResult:CBATTErrorRequestNotSupported]; return;
    }
    NSData *payload = [NSData dataWithBytes:messages.front().data() length:messages.front().size()];
    if (request.offset > payload.length) {
        [peripheral respondToRequest:request withResult:CBATTErrorInvalidOffset]; return;
    }
    self.peer = request.central;
    request.value = [payload subdataWithRange:NSMakeRange(request.offset, payload.length - request.offset)];
    [peripheral respondToRequest:request withResult:CBATTErrorSuccess];
    GBLog(@"peripheral callback: read request served");
    if (request.offset == 0 && !self.readServed) {
        self.readServed = YES;
        if (self.subscribed) [self sendNotification];
        else [self watch:GBPPeer label:@"peer subscription after read"];
    }
}
- (void)peripheralManager:(CBPeripheralManager *)peripheral didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
    // Read/notify-only contract. Respond once to the ATT write transaction.
    if (requests.count) [peripheral respondToRequest:requests.firstObject withResult:CBATTErrorWriteNotPermitted];
}
- (void)peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
    (void)peripheral;
    if (self.session.finished || characteristic != self.characteristic) return;
    if (self.peer && ![self.peer.identifier isEqual:central.identifier]) return;
    if (self.subscribed) return;
    self.peer = central;
    self.subscribed = YES;
    GBLog(@"peripheral callback: peer subscribed");
    if (self.readServed) [self sendNotification];
    else [self watch:GBPPeer label:@"peer read after subscription"];
}
- (void)sendNotification {
    if (self.session.finished || self.notificationQueued || !self.readServed || !self.subscribed) return;
    if (self.phase != GBPQueue) [self watch:GBPQueue label:@"notification batch queue availability"];
    // Check the entire batch before sending any prefix. Never rely on Core Bluetooth truncation.
    for (const auto& bytes : messages) {
        if (bytes.size() > self.peer.maximumUpdateValueLength) {
            [self fail:@"Notification failure: payload exceeds peer maximumUpdateValueLength; no fragmentation"]; return;
        }
    }
    while (nextMessage < messages.size()) {
        const auto& bytes = messages[nextMessage];
        NSData *value = [NSData dataWithBytes:bytes.data() length:bytes.size()];
        if (![self.manager updateValue:value forCharacteristic:self.characteristic onSubscribedCentrals:@[self.peer]]) {
            GBLog(@"peripheral: backpressure, waiting for readiness callback");
            return; // Retain this exact message/index, keep original batch deadline.
        }
        ++nextMessage;
    }
    self.notificationQueued = YES;
    GBLog(@"peripheral: batch queued, delivery not acknowledged");
    [self watch:GBPUnsubscribe label:@"peer disconnect/unsubscribe after notifications"];
}
- (void)peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
    (void)peripheral;
    if (self.phase == GBPQueue) [self sendNotification];
}
- (void)peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
    (void)peripheral;
    if (self.session.finished || characteristic != self.characteristic ||
        ![central.identifier isEqual:self.peer.identifier]) return;
    self.subscribed = NO;
    if (!self.readServed || !self.notificationQueued) {
        [self fail:@"Peer unsubscribed before read/notification sequence completed"]; return;
    }
    GBLog(@"peripheral callback: peer unsubscribed");
    [self.session finish];
}
- (void)cleanup {
    [self.session finish];
    if (self.manager.state == CBManagerStatePoweredOn) {
        // Central now cancels while subscribed, so unsubscribe normally follows
        // its disconnect. Keep the service alive through a bounded grace period.
        GBDrainPeripheral(self.manager, self.session);
        [self.manager removeAllServices];
    }
    self.manager.delegate = nil;
    self.peer = nil;
    self.service = nil;
    self.characteristic = nil;
    self.manager = nil;
    GBLog(@"peripheral cleanup: timer invalidated, delegate detached, objects released");
}
@end

void BlePeripheral::publish(const BleConfig& config, const std::vector<std::string>& messages) {
    validateBleConfig(config);
    validateBleCount(messages.size());
    for (const auto& bytes : messages) validateBlePayload(bytes);
    GBValidateRun(config.timeout);
    @autoreleasepool {
        GBBlePeripheralDelegate *delegate = [GBBlePeripheralDelegate new];
        delegate.session = [GBBleSession new];
        delegate->config = config;
        delegate->messages = messages;
        delegate.session.seconds = config.timeout.count() / 1000.0;
        @try {
            GBLog([@"peripheral initial authorization=" stringByAppendingString:GBAuthorization()]);
            [delegate watch:GBPInitializing label:@"peripheral manager initialization/authorization"];
            delegate.manager = [[CBPeripheralManager alloc] initWithDelegate:delegate queue:nil
                options:@{CBPeripheralManagerOptionShowPowerAlertKey: @NO}];
            GBWait(delegate.session);
        } @finally { [delegate cleanup]; }
        GBThrowIfFailed(delegate.session);
        GBLog(@"PERIPHERAL SESSION COMPLETE: batch queued, unsubscribe observed; verify delivery at Central");
    }
}

void BlePeripheral::run(std::chrono::milliseconds timeout) {
    BleConfig config;
    config.timeout = timeout;
    publish(config, {ble_experiment::PAYLOAD});
}
