#include <transport/ble/central.hpp>
#import <transport/ble/platform/macos/session.hpp>
#include <stdexcept>

typedef NS_ENUM(NSInteger, GBCentralPhase) {
    GBInitializing, GBScanning, GBConnecting, GBServices, GBCharacteristics,
    GBReading, GBSubscribing, GBNotification, GBDisconnecting
};

@interface GBBleCentralDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate> {
@public
    BleConfig config;
    BleReceived received;
    std::size_t expected;
}
@property(nonatomic, strong) GBBleSession *session;
@property(nonatomic, strong) CBCentralManager *manager;
@property(nonatomic, strong) CBPeripheral *peer;
@property(nonatomic, strong) CBCharacteristic *characteristic;
@property(nonatomic) GBCentralPhase phase;
@property(nonatomic) BOOL readVerified;
@property(nonatomic) BOOL subscriptionConfirmed;
@property(nonatomic) BOOL notificationVerified;
- (void)watch:(GBCentralPhase)phase label:(NSString *)label;
- (void)fail:(NSString *)message;
- (void)disconnect;
- (void)cleanup;
@end

@implementation GBBleCentralDelegate
- (void)watch:(GBCentralPhase)phase label:(NSString *)label {
    self.phase = phase;
    __weak GBBleCentralDelegate *weakSelf = self;
    [self.session watch:label timeout:^{
        GBBleCentralDelegate *owner = weakSelf;
        if (!owner || owner.session.finished) return;
        NSString *message = [@"Timeout: " stringByAppendingString:label];
        if (phase == GBDisconnecting) {
            owner.session.error = owner.session.error
                ? [owner.session.error stringByAppendingFormat:@"; %@", message] : message;
            [owner.session finish];
        } else [owner fail:message];
    }];
}
- (void)fail:(NSString *)message {
    if (self.session.finished) return;
    if (!self.session.error) self.session.error = message;
    if (self.phase == GBDisconnecting) return; // Keep the existing cancellation deadline.
    if (self.manager.state == CBManagerStatePoweredOn) [self.manager stopScan];
    if (self.peer && self.peer.state != CBPeripheralStateDisconnected) [self disconnect];
    else [self.session finish];
}
- (void)disconnect {
    [self watch:GBDisconnecting label:@"local disconnect confirmation"];
    [self.manager cancelPeripheralConnection:self.peer];
}
- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    if (self.session.finished) return;
    GBLog([NSString stringWithFormat:@"central callback: state=%@ authorization=%@",
           GBState(central.state), GBAuthorization()]);
    NSString *problem = GBManagerProblem(central.state);
    if (problem) { [self fail:problem]; return; }
    if (central.state != CBManagerStatePoweredOn) {
        if (self.phase != GBInitializing) [self fail:@"Bluetooth became unavailable during session"];
        return;
    }
    if (self.phase != GBInitializing) return;
    [self watch:GBScanning label:@"discovery (no matching peripheral)"];
    [central scanForPeripheralsWithServices:@[[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.service.c_str()]]] options:nil];
    GBLog([NSString stringWithFormat:@"central scan initiated: isScanning=%@", central.isScanning ? @"YES" : @"NO"]);
}
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral
    advertisementData:(NSDictionary<NSString *, id> *)advertisementData RSSI:(NSNumber *)RSSI {
    (void)advertisementData; (void)RSSI;
    if (self.session.finished || self.phase != GBScanning) return;
    GBLog(@"central callback: matching peripheral discovered");
    self.peer = peripheral;
    peripheral.delegate = self;
    [central stopScan];
    [self watch:GBConnecting label:@"connection"];
    [central connectPeripheral:peripheral options:nil];
}
- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    (void)central;
    if (self.session.finished || peripheral != self.peer) return;
    if (self.phase == GBDisconnecting) { [self.manager cancelPeripheralConnection:peripheral]; return; }
    if (self.phase != GBConnecting) return;
    GBLog(@"central callback: connected");
    [self watch:GBServices label:@"service discovery"];
    [peripheral discoverServices:@[[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.service.c_str()]]]];
}
- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    (void)central;
    if (self.session.finished || peripheral != self.peer) return;
    if (self.phase == GBDisconnecting && self.session.error) { [self.session finish]; return; }
    [self fail:[NSString stringWithFormat:@"Connection failure: %@", error ? error.description : @"unknown error"]];
}
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (self.session.finished || peripheral != self.peer || self.phase != GBServices) return;
    if (error) { [self fail:[@"Service discovery failure: " stringByAppendingString:error.description]]; return; }
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.service.c_str()]]]) {
            GBLog(@"central callback: expected service discovered");
            [self watch:GBCharacteristics label:@"characteristic discovery"];
            [peripheral discoverCharacteristics:@[[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.characteristic.c_str()]]] forService:service];
            return;
        }
    }
    [self fail:@"Service discovery failure: expected UUID missing"];
}
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    if (self.session.finished || peripheral != self.peer || self.phase != GBCharacteristics) return;
    if (error) { [self fail:[@"Characteristic discovery failure: " stringByAppendingString:error.description]]; return; }
    for (CBCharacteristic *value in service.characteristics) {
        if ([value.UUID isEqual:[CBUUID UUIDWithString:[NSString stringWithUTF8String:config.characteristic.c_str()]]]) {
            if (!(value.properties & CBCharacteristicPropertyRead) || !(value.properties & CBCharacteristicPropertyNotify)) {
                [self fail:@"Characteristic discovery failure: read/notify properties required"]; return;
            }
            self.characteristic = value;
            GBLog(@"central callback: expected read/notify characteristic discovered");
            [self watch:GBReading label:@"characteristic read"];
            [peripheral readValueForCharacteristic:value];
            return;
        }
    }
    [self fail:@"Characteristic discovery failure: expected UUID missing"];
}
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)value error:(NSError *)error {
    if (self.session.finished || peripheral != self.peer || value != self.characteristic) return;
    if (self.phase != GBReading && self.phase != GBSubscribing && self.phase != GBNotification) return;
    NSString *operation = self.phase == GBReading ? @"Read" : @"Notification";
    if (error || !value.value || value.value.length > BLE_MAX_PAYLOAD) {
        [self fail:[NSString stringWithFormat:@"%@ failure: %@", operation, error ? error.description : @"missing or oversized payload"]]; return;
    }
    try {
        std::string bytes;
        if (value.value.length) bytes.assign(static_cast<const char *>(value.value.bytes), value.value.length);
        if (self.phase == GBReading) {
            received.readValue = bytes;
            self.readVerified = YES;
            GBLog(@"central callback: initial binary read received");
            [self watch:GBSubscribing label:@"notification subscription"];
            [peripheral setNotifyValue:YES forCharacteristic:value];
        } else {
            if (received.messages.size() >= expected) { [self fail:@"Unexpected extra notification"]; return; }
            received.messages.push_back(std::move(bytes));
            self.notificationVerified = received.messages.size() == expected;
            if (self.notificationVerified && self.subscriptionConfirmed) [self disconnect];
        }
    } catch (...) {
        // Do not format/log/allocate while handling an allocation failure. Stop
        // processing now; receive()'s finally block cancels and detaches resources.
        GBBleSession *session = self.session;
        if (!session->cppFailure) session->cppFailure = std::current_exception();
        [session finish];
    }
}
- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)value error:(NSError *)error {
    if (self.session.finished || peripheral != self.peer || value != self.characteristic) return;
    if (self.phase != GBSubscribing) return;
    if (error) { [self fail:[@"Notification state failure: " stringByAppendingString:error.description]]; return; }
    if (!value.isNotifying) { [self fail:@"Notification failure: subscription not enabled"]; return; }
    self.subscriptionConfirmed = YES;
    GBLog(@"central callback: subscription confirmed");
    if (self.notificationVerified) [self disconnect];
    else [self watch:GBNotification label:@"notification batch delivery"];

}
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    (void)central;
    if (self.session.finished || peripheral != self.peer) return;
    if (self.phase != GBDisconnecting || error || !self.readVerified ||
        !self.notificationVerified || !self.subscriptionConfirmed) {
        if (!self.session.error) self.session.error = [NSString stringWithFormat:@"Disconnection before successful completion: %@", error ? error.description : @"incomplete exchange"];
    } else GBLog(@"central callback: clean local disconnect confirmed");
    [self.session finish];
}
- (void)cleanup {
    [self.session finish];
    if (self.manager.state == CBManagerStatePoweredOn) [self.manager stopScan];
    if (self.peer && self.peer.state != CBPeripheralStateDisconnected) {
        [self.manager cancelPeripheralConnection:self.peer];
    }
    self.peer.delegate = nil;
    self.manager.delegate = nil;
    self.characteristic = nil;
    self.peer = nil;
    self.manager = nil;
    GBLog(@"central cleanup: timer invalidated, scan stopped where powered on, delegates detached, objects released");
}
@end

BleReceived BleCentral::receive(const BleConfig& config, std::size_t count) {
    validateBleConfig(config);
    validateBleCount(count);
    GBValidateRun(config.timeout);
    @autoreleasepool {
        GBBleCentralDelegate *delegate = [GBBleCentralDelegate new];
        delegate.session = [GBBleSession new];
        delegate->config = config;
        delegate->expected = count;
        delegate.session.seconds = config.timeout.count() / 1000.0;
        @try {
            GBLog([@"central initial authorization=" stringByAppendingString:GBAuthorization()]);
            [delegate watch:GBInitializing label:@"central manager initialization/authorization"];
            delegate.manager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil
                options:@{CBCentralManagerOptionShowPowerAlertKey: @NO}];
            GBWait(delegate.session);
        } @finally { [delegate cleanup]; }
        GBThrowIfFailed(delegate.session);
        GBLog(@"central: complete notification batch and local disconnect observed");
        return std::move(delegate->received);
    }
}

void BleCentral::run(std::chrono::milliseconds timeout) {
    BleConfig config;
    config.timeout = timeout;
    const auto result = receive(config, 1);
    if (result.readValue != ble_experiment::PAYLOAD || result.messages.front() != ble_experiment::PAYLOAD)
        throw std::runtime_error("BLE feasibility fixture mismatch");
    GBLog(@"CENTRAL EXCHANGE VERIFIED: exact fixture read and notification, local disconnect");
}
