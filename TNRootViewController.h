#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

@interface TNRootViewController : UIViewController
{
    UIColor *_backgroundColor;
    UIColor *_tintColor;

    UIButton *_goButton;
    UILabel *_titleLabel;
    UILabel *_infoLabel;

    AVAudioPlayer *_audioPlayer;
}
@end