#include "io/file_dialog.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace pixelscope::io {

std::optional<std::string> open_image_dialog() {
  @autoreleasepool {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Open image"];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];

    NSArray* extensions = @[
        @"png", @"jpg", @"jpeg",
        @"tif", @"tiff",
        @"dng",
        @"raw", @"bin", @"bayer"
    ];

    if (@available(macOS 11.0, *)) {
      NSMutableArray<UTType*>* contentTypes = [NSMutableArray array];
      for (NSString* ext in extensions) {
        UTType* type = [UTType typeWithFilenameExtension:ext];
        if (type != nil) {
          [contentTypes addObject:type];
        }
      }
      [panel setAllowedContentTypes:contentTypes];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      [panel setAllowedFileTypes:extensions];
#pragma clang diagnostic pop
    }

    if ([panel runModal] != NSModalResponseOK) {
      return std::nullopt;
    }

    NSURL* url = [[panel URLs] firstObject];
    if (url == nil) {
      return std::nullopt;
    }

    return std::string([[url path] UTF8String]);
  }
}

}  // namespace pixelscope::io
