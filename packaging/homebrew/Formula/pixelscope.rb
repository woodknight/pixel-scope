class Pixelscope < Formula
  desc "Cross-platform pixel inspection desktop viewer"
  homepage "https://github.com/woodknight/pixel-scope"
  url "https://github.com/woodknight/pixel-scope/releases/download/v0.1.0/pixelscope-0.1.0-macos-arm64.zip"
  version "0.1.0"
  sha256 "a4b54d710c4637ee9a35e33461d8cba9e4167b09484cb293b5a5bec037321e68"
  license "MIT"

  depends_on "libtiff"

  def install
    libexec.install Dir["*"]
    bin.write_exec_script libexec/"PixelScope-0.1.0-Darwin-arm64/bin/pixelscope"
  end

  test do
    assert_predicate bin/"pixelscope", :exist?
  end
end
