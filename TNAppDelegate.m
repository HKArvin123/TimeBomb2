#import "TNAppDelegate.h"
#import "TNRootViewController.h"

@implementation TNAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	_window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
	_window.rootViewController = [[TNRootViewController alloc] init];
	[_window makeKeyAndVisible];
	return YES;
}

@end
