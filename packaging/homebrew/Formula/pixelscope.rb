class Pixelscope < Formula
  desc "Cross-platform pixel inspection desktop viewer"
  homepage "https://github.com/woodknight/pixel-scope"
  url "https://github.com/woodknight/pixel-scope/releases/download/v0.1.4/pixelscope-0.1.4-macos-arm64.zip"
  version "0.1.4"
  sha256 "bb2486211a87bdbfe4f49b473433b3b5b3b25ab89534fc5e947c0d354bc19147"
  license "MIT"

  def install
    libexec.install Dir["*"]
    bin.write_exec_script libexec/"bin/pixelscope"

    # Create a macOS .app bundle so the app appears in Launchpad / Spotlight
    app = prefix/"PixelScope.app/Contents"
    (app/"MacOS").mkpath
    (app/"Resources").mkpath

    cp libexec/"share/pixelscope/icons/pixelscope.icns", app/"Resources/pixelscope.icns"

    (app/"MacOS/PixelScope").write <<~SCRIPT
      #!/bin/bash
      exec "#{libexec}/bin/pixelscope" "$@"
    SCRIPT
    (app/"MacOS/PixelScope").chmod 0755

    (app/"Info.plist").write <<~PLIST
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
        "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
      <dict>
        <key>CFBundleName</key>
        <string>PixelScope</string>
        <key>CFBundleDisplayName</key>
        <string>PixelScope</string>
        <key>CFBundleIdentifier</key>
        <string>com.woodknight.pixelscope</string>
        <key>CFBundleVersion</key>
        <string>#{version}</string>
        <key>CFBundleShortVersionString</key>
        <string>#{version}</string>
        <key>CFBundleExecutable</key>
        <string>PixelScope</string>
        <key>CFBundleIconFile</key>
        <string>pixelscope</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>NSHighResolutionCapable</key>
        <true/>
      </dict>
      </plist>
    PLIST
  end

  def caveats
    <<~EOS
      To add PixelScope to your Applications folder:
        ln -sf #{prefix}/PixelScope.app /Applications/PixelScope.app
      To remove the link:
        rm /Applications/PixelScope.app
    EOS
  end

  test do
    assert_predicate bin/"pixelscope", :exist?
    assert_predicate prefix/"PixelScope.app/Contents/MacOS/PixelScope", :executable?
  end
end
