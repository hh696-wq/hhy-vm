class Hhy < Formula
  desc "Flow-first scripting language for system automation"
  homepage "https://hhylang.dev"
  url "https://github.com/hh696-wq/hhy-vm/releases/download/v1.7.0/hhy-1.7.0-darwin-arm64.tar.gz"
  sha256 "6a2f6288957a27a08d18621053dcf735f803bd37245d57a5edb2ad91892683e2"
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
