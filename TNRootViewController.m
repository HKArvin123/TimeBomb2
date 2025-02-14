#import "TNRootViewController.h"
#import "trigger.h"

@implementation TNRootViewController

- (UIStatusBarStyle)preferredStatusBarStyle
{
	return UIStatusBarStyleLightContent;
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	_backgroundColor = [UIColor colorWithRed:(16.0 / 255.0) green:(23.0 / 255.0) blue:(39.0 / 255.0) alpha:1.0];
	_tintColor = [UIColor colorWithRed:1.0 green:(171.0 / 255.0) blue:(21.0 / 255.0) alpha:1.0];

	self.view.backgroundColor = _backgroundColor;
	self.view.tintColor = _tintColor;

	_titleLabel = [[UILabel alloc] init];
	_titleLabel.text = @"TimeBomb 2";
	_titleLabel.textAlignment = NSTextAlignmentCenter;
	_titleLabel.font = [UIFont systemFontOfSize:40 weight:UIFontWeightBold];
	_titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
	_titleLabel.textColor = _tintColor;

	_infoLabel = [[UILabel alloc] init];
	_infoLabel.textAlignment = NSTextAlignmentCenter;
	_infoLabel.font = [UIFont systemFontOfSize:30];
	_infoLabel.translatesAutoresizingMaskIntoConstraints = NO;
	_infoLabel.textColor = _tintColor;
	_infoLabel.numberOfLines = 0;

	_goButton = [UIButton buttonWithType:UIButtonTypeSystem];
	_goButton.titleLabel.font = [UIFont systemFontOfSize:30];
	[_goButton setTitle:@"Go" forState:UIControlStateNormal];
	[_goButton setTitleColor:_tintColor forState:UIControlStateNormal];
	_goButton.translatesAutoresizingMaskIntoConstraints = NO;
	[_goButton setTitleColor:_tintColor forState:UIControlStateNormal];
	[_goButton addTarget:self action:@selector(goPressed) forControlEvents:UIControlEventTouchUpInside];

	[self.view addSubview:_titleLabel];
	[self.view addSubview:_infoLabel];
	[self.view addSubview:_goButton];

	[NSLayoutConstraint activateConstraints:@[
		[_titleLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
		[_titleLabel.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:20],

		[_infoLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
		[_infoLabel.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],

		[_goButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
		[_goButton.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-20],
	]];

	NSURL *url = [NSURL fileURLWithPath:[[NSBundle mainBundle] pathForResource:@"yellow-repo" ofType:@"mp3"]];
	_audioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:nil];
	_audioPlayer.numberOfLoops = -1;
}

- (void)goPressed
{
	UIAlertController *infoAlert = [UIAlertController alertControllerWithTitle:@"Important" message:@"The upcoming Kernel panic has been sponsered by CyPwn!\nBig thanks to my #1 supporter @sudo1469.\n\nNOTE: This software is not liable for any damage or data loss caused by the kernel panic it will induce.\nUse on your own risk." preferredStyle:UIAlertControllerStyleAlert];
	UIAlertAction *understandAction = [UIAlertAction actionWithTitle:@"I Understand" style:UIAlertActionStyleDestructive handler:^(UIAlertAction * action){
		[_audioPlayer play];
		_goButton.enabled = NO;
		dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			trigger_spinlock_panic(20, ^(uint64_t idx, uint64_t cnt){
				dispatch_async(dispatch_get_main_queue(), ^(void) {
					@autoreleasepool {
						if (idx == cnt) {
							[_audioPlayer pause];
							_infoLabel.text = @"Device unsupported?";
							_goButton.enabled = YES;
						}
						else {
							_infoLabel.text = [NSString stringWithFormat:@"Working...\n%llu/%llu", idx, cnt];
						}
					}
				});
			});
		});
	}];
	UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil];
	[infoAlert addAction:understandAction];
	[infoAlert addAction:cancelAction];
	[self presentViewController:infoAlert animated:YES completion:nil];
}

@end
