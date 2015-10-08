/*****************************************************************************/
/** Kaleido scope for AviUtl  ver 0.01
 */
#define _USE_MATH_DEFINES
#include <cmath>
#include <utility>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include "filter.h"


/*---------------------------------------------------------------------------*/
/** FILTER_DLL structure
 */
char filtername[] = "KaleidoScope";
char filterinfo[] = "KaleidoScope for AviUtl ver 0.01 by MakKi";

#define track_N 4
TCHAR *track_name[] = { "x", "y", "size", "angle" };
int track_default[] = {  100,  100, 100,    0 };
int track_s[] =       {    0,    0,   1, -1800 };
int track_e[] =       { 1024, 1024, 500,  1800 };

#define check_N 1
TCHAR *check_name[] = { "show triangle" };
int check_default[] = { 0 };

#define tX     0
#define tY     1
#define tSIZE  2
#define tANGLE 3
#define cSHOW  0

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	NULL, NULL,	// window size.
	filtername,
	track_N,
	track_name, track_default,
	track_s, track_e,
	check_N,
	check_name, check_default,
	func_proc,
	NULL, NULL,	// func_init, func_exit,
	NULL,	// func_update,
	func_WndProc,
	NULL,NULL,
	NULL,NULL,
	filterinfo,
	NULL, NULL,	// func_save_start, func_save_end,
	NULL,NULL,NULL,
	NULL,
	NULL,	// func_is_saveframe,
	NULL, NULL,	// func_project_load, func_project_save,
	NULL,	// func_modify_title
	NULL,
	{ NULL,NULL }
};

/*===========================================================================*/
/** DLL Export
 */
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable( void )
{
	return &filter;
}

/*****************************************************************************/


class CoordCalc {
public:
	typedef std::pair<double,double> Coord;

protected:
	double ox,oy;
	double unit;

	double sin_th;
	double cos_th;

	double c_xtow, c_ytow;
	double c_xtoz, c_ytoz;
	double c_wtox, c_wtoy;
	double c_ztox, c_ztoy;

	int mod3(int x){
		return (x>=0)? x%3: (3+(x%3))%3;
	}

	static Coord ref_A(double w,double z){
		return (w+z<=1)? Coord(w,z): Coord(1.0-z,1.0-w);
	}
	static Coord ref_B(double w,double z){
		return (w+z<=1)? Coord(1.0-w-z,w): Coord(w+z-1.0,1.0-z);
	}
	static Coord ref_C(double w,double z){
		return (w+z<=1)? Coord(z,1.0-w-z): Coord(1.0-w,z+w-1.0);
	}

	static Coord (*ref[3][3])(double w,double z);

public:
	CoordCalc(double x,double y,double l,double th){
		ox = x;
		oy = y;
		unit = l;

		double sin_th = sin(th);
		double cos_th = cos(th);
		double root3 = sqrt(3.0);

		c_xtow = cos_th + sin_th/root3;
		c_ytow = sin_th - cos_th/root3;
		c_xtoz = -2.0 * sin_th / root3;
		c_ytoz = 2.0 * cos_th / root3;

		c_wtox = cos_th;
		c_ztox = (cos_th - root3*sin_th)/2;
		c_wtoy = sin_th;
		c_ztoy = (sin_th + root3*cos_th)/2;
	}

	Coord operator() (double x,double y){
		x -= ox;
		y -= oy;

		// 座標変換 (直交->三角)
		double w = (c_xtow * x + c_ytow * y) /unit;
		double z = (c_xtoz * x + c_ytoz * y) /unit;

		// 参照座標
		double fw = floor(w);
		double fz = floor(z);
		Coord r = ref[mod3((int)fz)][mod3((int)fw)](w-fw,z-fz);
		w = r.first * unit;
		z = r.second * unit;

		// 座標変換 (三角->直交)
		r.first  = c_wtox * w + c_ztox * z + ox;
		r.second = c_wtoy * w + c_ztoy * z + oy;

		return r;
	}
};

CoordCalc::Coord (*CoordCalc::ref[3][3])(double w,double z) = {
	{ref_A, ref_B, ref_C}, {ref_C, ref_A, ref_B}, {ref_B, ref_C, ref_A}
};


/*---------------------------------------------------------------------------*/
class Adjust {
	double x,x_,y,y_;
public:
	Adjust(double xx,double yy){
		x = xx;
		x_ = 1.0 - xx;
		y = yy;
		y_ = 1.0 - yy;
	}
	int operator()(double a,double b,double c,double d){
		return static_cast<int>(a*x_*y_ + b*x*y_ + c*x_*y + d*x*y);
	}
};


/*---------------------------------------------------------------------------*/
inline void plot(PIXEL_YC *ycp,int w,int h,int x,int y)
{
	if(0<=x && x<w && 0<=y && y<h){
		ycp += x + y * w;
		ycp->y = 2048;
		ycp->cb = 0;
		ycp->cr = 2048;
	}
}

inline void drow_line(PIXEL_YC *ycp,int w,int h,int x1,int y1,int x2,int y2)
{
	int sx = 1;
	int sy = 1;
	int dx = x2 - x1;
	if(dx<0){
		sx = -1;
		dx = -dx;
	}
	int dy = y2 - y1;
	if(dy<0){
		sy = -1;
		dy = -dy;
	}

	plot(ycp,w,h,x1,y1);
	int e,x,y;
	if(dx>dy){
		for(x=0,y=0,e=0;x<=dx;++x){
			e += dy;
			if(e>dx){
				e -= dx;
				++y;
			}
			plot(ycp,w,h,x1+(sx*x),y1+(sy*y));
		}
	}
	else{
		for(y=0,x=0,e=0;y<=dy;++y){
			e += dx;
			if(e>dy){
				e -= dy;
				++x;
			}
			plot(ycp,w,h,x1+(sx*x),y1+(sy*y));
		}
	}
}

BOOL show_triangle(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	double th = -fp->track[tANGLE]*M_PI/1800.0;
	int x1 = fp->track[tX];
	int y1 = fp->track[tY];
	int x2 = x1 + fp->track[tSIZE] * cos(th) + 0.5;
	int y2 = y1 + fp->track[tSIZE] * sin(th) + 0.5;
	int x3 = x1 + fp->track[tSIZE] * cos(th + M_PI/3);
	int y3 = y1 + fp->track[tSIZE] * sin(th + M_PI/3);

	drow_line(fpip->ycp_edit,fpip->max_w,fpip->h,x1,y1,x2,y2);
	drow_line(fpip->ycp_edit,fpip->max_w,fpip->h,x2,y2,x3,y3);
	drow_line(fpip->ycp_edit,fpip->max_w,fpip->h,x3,y3,x1,y1);

	return TRUE;
}


/*===========================================================================*/
/** Processing.
 */
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	if(fp->check[cSHOW])
		return show_triangle(fp,fpip);

	CoordCalc cc(fp->track[tX],fp->track[tY],fp->track[tSIZE],
				 -fp->track[tANGLE]*M_PI/1800.0);

	PIXEL_YC *dst = fpip->ycp_temp;
	int pitch = fpip->max_w;

	for(int h=0;h<fpip->h;++h){
		for(int w=0;w<fpip->w;++w){
			CoordCalc::Coord ref = cc(w,h);

			int ww = static_cast<int>(floor(ref.first));
			int hh = static_cast<int>(floor(ref.second));

			if( ww<0 || ww>fpip->w-1 || hh<0 || hh>fpip->h-1){
				dst->y = dst->cb = dst->cr = 0;
			}
			else{
				PIXEL_YC *src = fpip->ycp_edit + (int)ww + (int)hh * pitch;
				Adjust adjust(ref.first-ww, ref.second-hh);
				dst->y =  adjust(src->y, (src+1)->y, (src+pitch)->y, (src+pitch+1)->y);
				dst->cb = adjust(src->cb,(src+1)->cb,(src+pitch)->cb,(src+pitch+1)->cb);
				dst->cr = adjust(src->cr,(src+1)->cr,(src+pitch)->cr,(src+pitch+1)->cr);
			}
			++dst;
		}
		dst += pitch - fpip->w;
	}

	dst = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = dst;

	return TRUE;
}

/*===========================================================================*/
/** Window procedure.
 * send message to main window.
 */
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:
		case WM_KEYDOWN:
		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wParam, lParam);
			break;
	}

	return FALSE;
}


/*****************************************************************************/
/** DLLMain.
 * iniファイルの読み込み
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved)
{
#define FILTER_NAME_MAX  32
#define FILTER_TRACK_MAX 16
#define FILTER_CHECK_MAX 32

	//FILTER filter = ::filter;
	static char *strings[1+track_N+check_N];
	char section[32];
	char ini_name[MAX_PATH];
	int i;

	switch(fdwReason){
	case DLL_PROCESS_ATTACH:	// 開始時

		// iniファイル名を取得
		GetModuleFileName(hinstDLL,ini_name,MAX_PATH-4);
		strcat(ini_name,".ini");

		// フィルタ名
		strings[0] = reinterpret_cast<char*>(malloc(FILTER_NAME_MAX));
		if(strings[0]==NULL) break;
		GetPrivateProfileString("main","name",filtername,strings[0],FILTER_NAME_MAX,ini_name);
		filter.name = strings[0];

		// track bar
		for(i=0;i<track_N;++i){
			wsprintf(section,"Track%d",i);
			// name
			strings[i+1] = reinterpret_cast<char*>(malloc(FILTER_TRACK_MAX));
			if(strings[i+1]){
				GetPrivateProfileString(section,"string",filter.track_name[i],strings[i+1],FILTER_TRACK_MAX,ini_name);
				filter.track_name[i] = strings[i+1];
			}
			// default value
			filter.track_default[i] = GetPrivateProfileInt(section,"default",filter.track_default[i],ini_name);
			// minimum
			filter.track_s[i] = GetPrivateProfileInt(section,"min",filter.track_s[i],ini_name);
			// maximum
			filter.track_e[i] = GetPrivateProfileInt(section,"max",filter.track_e[i],ini_name);
		}

		// チェック名
		for(i=0;i<check_N;i++){
			wsprintf(section,"Check%d",i);
			strings[i+track_N+1] = reinterpret_cast<char*>(malloc(FILTER_CHECK_MAX));
			if(strings[i+track_N+1]){
		   		GetPrivateProfileString(section,"string",filter.check_name[i],strings[i+track_N+1],FILTER_CHECK_MAX,ini_name);
			filter.check_name[i] = strings[i+track_N+1];
			}
		}
		break;

	case DLL_PROCESS_DETACH:	// 終了時
		// stringsを破棄
		for(i=0;i<1+track_N+check_N && strings[i];i++)
			free(strings[i]);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

//*/
