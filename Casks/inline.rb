cask "inline" do
  version "1.1"
  sha256 "d2ecef3a6e4fedb8aeb66b3b4ac7c1197ea6c903ad6d0da81d304367d8801645"

  url "https://github.com/LaxExe/Inline-MacOS-Win32Build/releases/download/v#{version}/Inline-v#{version}-macOS.dmg"
  name "Inline"
  desc "Ultra-lightweight native macOS autocorrect engine"
  homepage "https://github.com/LaxExe/Inline-MacOS-Win32Build"

  livecheck do
    url :url
    strategy :github_latest
  end

  app "Inline.app"

  zap trash: [
    "~/Library/Application Support/Inline",
    "~/Library/Preferences/com.inline.mac.plist",
  ]
end
