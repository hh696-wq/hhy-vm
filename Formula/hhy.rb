class Hhy < Formula
  desc "Flow-first scripting language for system automation"
  homepage "https://hhylang.dev"
  url "https://github.com/hh696-wq/hhy-vm/releases/download/v1.1.4/hhy-1.1.4-darwin-arm64.tar.gz"
  sha256 "6f67854d67a0938ba6e62e615a2d83f167fc4542757ae3837a67c35e37494508"
  license "Apache-2.0"

  depends_on arch: :arm64

  def install
    libexec.install Dir["*"]
    (bin/"hhy").write_exec_script libexec/"bin/hhy"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/hhy --version")
    (testpath/"hello.hhy").write <<~HHY
      ["Flow", "Pipe", "System"]
          |> print
    HHY
    assert_match "Flow", shell_output("#{bin}/hhy run #{testpath}/hello.hhy")
  end
end
