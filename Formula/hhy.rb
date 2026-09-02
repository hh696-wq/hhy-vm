class Hhy < Formula
  desc "Flow-first scripting language for system automation"
  homepage "https://hhylang.dev"
  url "https://github.com/hh696-wq/hhy-vm/releases/download/v1.3.11/hhy-1.3.11-darwin-arm64.tar.gz"
  sha256 "9e11b02c5b040fcd91938109cb3eebdc6f27d197ff336812e460b339faf804a7"
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
