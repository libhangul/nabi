#ifndef __HANGUL_H_
#define __HANGUL_H_

#define HCF	0x115f		/* hangul choseong  filler */
#define HJF	0x1160		/* hangul jungseong filler */

#define hangul_is_choseong(ch)	((ch) >= 0x1100 && (ch) <= 0x1159)
#define hangul_is_jungseong(ch)	((ch) >= 0x1161 && (ch) <= 0x11a2)
#define hangul_is_jongseong(ch)	((ch) >= 0x11a7 && (ch) <= 0x11f9)

wchar_t hangul_choseong_to_cjamo(wchar_t ch);
wchar_t hangul_jungseong_to_cjamo(wchar_t ch);
wchar_t hangul_jongseong_to_cjamo(wchar_t ch);

wchar_t hangul_choseong_to_jongseong(wchar_t ch);
wchar_t hangul_jongseong_to_choseong(wchar_t ch);
void    hangul_jongseong_dicompose(wchar_t ch, wchar_t* jong, wchar_t* cho);

wchar_t hangul_jamo_to_syllable(wchar_t choseong,
				wchar_t jungseong,
				wchar_t jongseong);
wchar_t hangul_ucs_to_ksc(wchar_t choseong,
			  wchar_t jungseong,
			  wchar_t jongseong);

#endif /* __HANGUL_H_ */
/* vim: set ts=8 sw=4 : */
