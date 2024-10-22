#include "imgui.h"
#include "imgui_internal.h"
#include "winux.hpp"
using namespace std;
using namespace winux;
using namespace ImGui;

int wmain( int argc, wchar_t** argv )
{
    Locale loc;
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    auto ctx = CreateContext();
    builder.AddRanges( ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon() );

    TextArchive taWords;
    UnicodeString strWords;
    UnicodeString word, nl;
    size_t k;
    k = 0;
    taWords.load( String(L"hanzi.txt"), true, "UTF-16LE" );
    strWords = taWords.toString<wchar>();
    while ( StrGetLine( &word, strWords, &k, &nl ) )
    {
        builder.AddChar(word[0]);
    }
    k = 0;
    taWords.load( String(L"hanzi2.txt"), true, "UTF-16LE" );
    strWords = taWords.toString<wchar>();
    while ( StrGetLine( &word, strWords, &k, &nl ) )
    {
        builder.AddChar(word[0]);
    }
    builder.BuildRanges(&ranges);
    ImWchar * CharsetRanges = ranges.Data;

    {
        String content;
        content += Format(L"static ImWchar app_full_ranges[] = {\n    ");
        auto i = 0, n = 0;
        for ( ; CharsetRanges[i]; i += 2 )
        {
            ImWchar r1 = CharsetRanges[i], r2 = CharsetRanges[i + 1];
            n += r2 - r1 + 1;
            if ( (i/2) % 8 == 0 && i != 0 )
            {
                content += L"\n    ";
            }
            content += Format(L"0x%04x, ", r1);
            content += Format(L"0x%04x, ", r2);
        }
        content += Format( L"0 " );
        content += Format(L"};\n");

        FilePutString(L"char-ranges.inl", content);
        wprintf(L"%d\n", n);
    }
    //{
    //    auto i = 0, n = 0;
    //    for ( ; CharsetRanges[i]; i += 2 )
    //    {
    //        ImWchar r1 = CharsetRanges[i], r2 = CharsetRanges[i + 1];
    //        n += r2 - r1 + 1;
    //        /*for ( auto c = CharsetRanges[i]; c <= CharsetRanges[i + 1]; c++ )
    //        {
    //            wprintf( L"%c[%x] ", c, c );
    //        }
    //        wprintf(L"\n");*/
    //        wprintf( L"%c[%x] ~ %c[%x] : %d\n", r1, r1, r2, r2, r2 - r1 + 1 );
    //    }
    //    wprintf( L"%d, %d\n", i, n );
    //}

    return 0;
}
