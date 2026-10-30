cask "inline" do
  version "1.1"
  sha256 "5b6c07246f64c88e32f48a494b4bbf10cc3c3ec2663486ed4b877a40e0af2724"

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
