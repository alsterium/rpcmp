#include "rpcmp/utility/metadata.hpp"
#include "test_support.hpp"

#include <string>
#include <string_view>

using rpcmp::utility::normalize_utf8;
using rpcmp::utility::select_mdx_title;
using rpcmp::utility::TextError;
using rpcmp::utility::TitleFallback;
using rpcmp::utility::utf16_to_utf8;

int main() {
  rpcmp::test::Suite suite;
  // Independent Unicode canonical equivalences; deliberately not NFKC.
  RPCMP_CHECK(suite, normalize_utf8(u8"か\u3099").text == u8"が");
  RPCMP_CHECK(suite, normalize_utf8(u8"e\u0301").text == u8"é");
  RPCMP_CHECK(suite, normalize_utf8(u8"\u1100\u1161").text == u8"가");
  RPCMP_CHECK(suite, normalize_utf8(u8"\u212b").text == u8"Å");
  RPCMP_CHECK(suite, normalize_utf8(u8"ｶﾞＡ①").text == u8"ｶﾞＡ①");
  RPCMP_CHECK(suite, normalize_utf8(" A\t ").text == " A\t ");
  RPCMP_CHECK(suite, normalize_utf8("").ok());
  RPCMP_CHECK(suite, normalize_utf8(u8"\u0378").text == u8"\u0378");
  for (const auto* invalid : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xe3\x81"})
    RPCMP_CHECK(suite, normalize_utf8(invalid).error == TextError::InvalidEncoding);
  RPCMP_CHECK(suite, normalize_utf8(std::string_view{"a\0b", 3}).error == TextError::EmbeddedNul);
  RPCMP_CHECK(suite, normalize_utf8(std::string(4096, 'x')).text.size() == 4096);
  RPCMP_CHECK(suite, normalize_utf8(std::string(4097, 'x')).error == TextError::SizeLimit);
  // U+0344 canonically expands into U+0308 U+0301; size is checked after NFC too.
  std::string expanding;
  for (int i = 0; i < 1025; ++i)
    expanding += u8"\u0344";
  RPCMP_CHECK(suite, normalize_utf8(expanding).error == TextError::SizeLimit);

  RPCMP_CHECK(suite, utf16_to_utf8(u"日本語\U0001f600").text == u8"日本語\U0001f600");
  RPCMP_CHECK(suite,
              utf16_to_utf8(std::u16string{char16_t{0xd800}}).error == TextError::InvalidEncoding);
  RPCMP_CHECK(suite,
              utf16_to_utf8(std::u16string{char16_t{0xdc00}}).error == TextError::InvalidEncoding);
  RPCMP_CHECK(suite, utf16_to_utf8(std::u16string{char16_t{0xd800}, u'A'}).error ==
                         TextError::InvalidEncoding);
  RPCMP_CHECK(suite,
              utf16_to_utf8(std::u16string_view{u"a\0b", 3}).error == TextError::EmbeddedNul);
  RPCMP_CHECK(suite, utf16_to_utf8(std::u16string(4096, u'x')).text.size() == 4096);
  RPCMP_CHECK(suite, utf16_to_utf8(std::u16string(1366, u'あ')).error == TextError::SizeLimit);
  RPCMP_CHECK(suite, utf16_to_utf8(std::u16string(4097, u'x')).error == TextError::SizeLimit);

  const auto mapped = select_mdx_title("\x82\xa0\x81\x60\xb6", "wrong");
  RPCMP_CHECK(suite, mapped.title.text == u8"あ～ｶ");
  RPCMP_CHECK(suite, mapped.fallback == TitleFallback::None);
  RPCMP_CHECK(suite, select_mdx_title(" \tA\r\nB\x81\x40", "wrong").title.text == "A  B");
  for (const auto* invalid : {"\x82", "\x82\x20", "\x80", "\xf0\x40"}) {
    const auto selected = select_mdx_title(invalid, u8"か\u3099");
    RPCMP_CHECK(suite, selected.title.text == u8"が");
    RPCMP_CHECK(suite, selected.fallback == TitleFallback::InvalidEncoding);
  }
  RPCMP_CHECK(suite, select_mdx_title("\t \x81\x40", "file").fallback == TitleFallback::Empty);
  RPCMP_CHECK(suite, select_mdx_title("\x1b", "file").fallback == TitleFallback::ControlCharacter);
  RPCMP_CHECK(suite, select_mdx_title(std::string_view{"a\0b", 3}, "file").fallback ==
                         TitleFallback::ControlCharacter);
  RPCMP_CHECK(suite, select_mdx_title("", "").title.error == TextError::Empty);
  RPCMP_CHECK(suite, select_mdx_title("", "\xff").title.error == TextError::InvalidEncoding);
  RPCMP_CHECK(suite,
              select_mdx_title(std::string(4097, 'a'), "file").title.error == TextError::SizeLimit);
  std::string wide;
  for (int i = 0; i < 1366; ++i)
    wide += "\x82\xa0";
  const auto too_large = select_mdx_title(wide, "fallback must not hide capacity failure");
  RPCMP_CHECK(suite, too_large.title.error == TextError::SizeLimit);
  RPCMP_CHECK(suite, too_large.title.text.empty());
  RPCMP_CHECK(suite, too_large.fallback == TitleFallback::None);
  return suite.finish("host metadata");
}
