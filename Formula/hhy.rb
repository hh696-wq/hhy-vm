class Hhy < Formula
  desc "Flow-first scripting language for system automation"
  homepage "https://hhylang.dev"
  url "https://github.com/hh696-wq/hhy-vm/releases/download/v1.3.6/hhy-1.3.6-darwin-arm64.tar.gz"
  sha256 "a816851333134eb57f91b89f8054d23dbca22cd8b1b790c708bff7b5799f6df1"
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
