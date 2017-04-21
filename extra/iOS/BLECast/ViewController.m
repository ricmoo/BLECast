//
//  ViewController.m
//  BLECast
//
//  Created by Richard Moore on 2017-04-11.
//  Copyright © 2017 RicMoo. All rights reserved.
//

#import "ViewController.h"

#import "BLECast.h"


@interface ViewController () <BLECastDelegate, UITextFieldDelegate> {
    BLECast *_bleCast;
    UILabel *_currentPayload;
    UITextField *_textField;
}

@end


@implementation ViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"BLECast Demo";
        
        self.edgesForExtendedLayout = UIRectEdgeNone;
        
        self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Start"
                                                                                  style:UIBarButtonItemStylePlain
                                                                                 target:self
                                                                                 action:@selector(startBroadcasting)];
    }
    return self;
}

- (void)startBroadcasting {
    uint8_t key[] = { 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 2, 2, 3, 3, 4, 4 };
    
    _bleCast = [BLECast bleCastWithKey:[NSData dataWithBytes:key length:sizeof(key)]
                                  data:[_textField.text dataUsingEncoding:NSUTF8StringEncoding]];
    _bleCast.delegate = self;
    [_bleCast start];
    
    _textField.enabled = NO;

    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Stop"
                                                                              style:UIBarButtonItemStylePlain
                                                                             target:self
                                                                             action:@selector(stopBroadcasting)];
}

- (void)stopBroadcasting {
    [_bleCast stop];
    _bleCast = nil;
    
    _textField.enabled = YES;
    _currentPayload.text = @"";

    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Start"
                                                                              style:UIBarButtonItemStylePlain
                                                                             target:self
                                                                             action:@selector(startBroadcasting)];
}

- (void)loadView {
    [super loadView];
    
    _textField = [[UITextField alloc] initWithFrame:CGRectMake(0.0, 100.0, self.view.frame.size.width, 44.0f)];
    _textField.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin;
    _textField.delegate = self;
    _textField.textAlignment = NSTextAlignmentCenter;
    _textField.placeholder = @"enter message here";
    _textField.text = @"Hello";
    [self.view addSubview:_textField];
    
    _currentPayload = [[UILabel alloc] initWithFrame:CGRectMake(0, self.view.frame.size.height - 44.0f, self.view.frame.size.width, 44.0f)];
    _currentPayload.autoresizingMask = UIViewAutoresizingFlexibleTopMargin;
    _currentPayload.font = [UIFont fontWithName:@"Menlo-Regular" size:16.0f];
    _currentPayload.textAlignment = NSTextAlignmentCenter;
    [self.view addSubview:_currentPayload];
    
}

- (void)bleCast:(BLECast *)bleCast didHopPayload:(NSData *)payload index:(uint8_t)index {
    
    const char HexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    uint8_t *payloadBytes = (uint8_t*)payload.bytes;
    char hexString[33];
    for (uint8_t i = 0; i < payload.length; i++) {
        hexString[2 * i + 0] = HexChars[(payloadBytes[i] >> 4) & 0x0f];
        hexString[2 * i + 1] = HexChars[(payloadBytes[i] >> 0) & 0x0f];
    }
    hexString[32] = 0;
    
    _currentPayload.text = [NSString stringWithUTF8String:hexString];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [textField resignFirstResponder];
    return NO;
}

@end
