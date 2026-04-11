class Pixelscope < Formula
  desc "Cross-platform pixel inspection desktop viewer"
  homepage "https://github.com/woodknight/pixel-scope"
  url "https://github.com/woodknight/pixel-scope/releases/download/v0.1.4/pixelscope-0.1.4-macos-arm64.zip"
  version "0.1.4"
  sha256 "bb2486211a87bdbfe4f49b473433b3b5b3b25ab89534fc5e947c0d354bc19147"
  license "MIT"

  def install
    libexec.install Dir["*"]
    bin.write_exec_script libexec/"PixelScope-0.1.4-Darwin-arm64/bin/pixelscope"
  end

  test do
    assert_predicate bin/"pixelscope", :exist?
  end
end
