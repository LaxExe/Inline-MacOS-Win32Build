cask "inline" do
  version "1.1"
  sha256 "a18fbfc1d0d39fed7720fb5b850dd051780a5752cb658688a17b938afbab7973"

  url "https://github.com/LaxExe/Inline-MacOS-Win32Build/releases/download/v#{version}/Inline-v#{version}-macOS.dmg"
  name "Inline"
  desc "Ultra-lightweight native macOS autocorrect engine"
  homepage "https://github.com/LaxExe/Inline-MacOS-Win32Build"

  livecheck do
    url :url
    strategy :github_latest
  end

  app "Inline.app"

  postflight do
    system_command "/usr/bin/xattr",
                   args: ["-cr", "#{appdir}/Inline.app"],
                   sudo: false
  end

  zap trash: [
    "~/Library/Application Support/Inline",
    "~/Library/Preferences/com.inline.mac.plist",
  ]
end
