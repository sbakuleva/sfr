#define GEOS_USE_ONLY_R_API // avoid using non-thread-safe GEOSxx functions without _r extension.
#include <geos_c.h>

#if GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 5
#  define HAVE350
#else
# if GEOS_VERSION_MAJOR > 3
#  define HAVE350
# endif
#endif

#include <Rcpp.h>

#include "wkb.h"

static void __errorHandler(const char *fmt, ...) { // #nocov start

	char buf[BUFSIZ], *p;
	va_list(ap);
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	p = buf + strlen(buf) - 1;
	if(strlen(buf) > 0 && *p == '\n') *p = '\0';

	Rcpp::Function error("stop");
	error(buf);

	return; // #nocov end
}

static void __warningHandler(const char *fmt, ...) {

	char buf[BUFSIZ], *p;
	va_list(ap);
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	p = buf + strlen(buf) - 1;
	if(strlen(buf) > 0 && *p == '\n') *p = '\0';

	Rcpp::Function warning("warning");
	warning(buf);
	
	return;
}

static void __countErrorHandler(const char *fmt, void *userdata) {
	int *i = (int *) userdata;
	*i = *i + 1;
}

static void __emptyNoticeHandler(const char *fmt, void *userdata) { }

GEOSContextHandle_t CPL_geos_init(void) {
#ifdef HAVE350
	GEOSContextHandle_t ctxt = GEOS_init_r();
	GEOSContext_setNoticeHandler_r(ctxt, __warningHandler);
	GEOSContext_setErrorHandler_r(ctxt, __errorHandler);
	return ctxt;
#else
	return initGEOS_r((GEOSMessageHandler) __warningHandler, (GEOSMessageHandler) __errorHandler);
#endif
}

void CPL_geos_finish(GEOSContextHandle_t ctxt) {
#ifdef HAVE350
	GEOS_finish_r(ctxt);
#else
	finishGEOS_r(ctxt);
#endif
}

std::vector<GEOSGeom> geometries_from_sfc(GEOSContextHandle_t hGEOSCtxt, Rcpp::List sfc, int *dim = NULL) {

	double precision = sfc.attr("precision");

	Rcpp::List wkblst = CPL_write_wkb(sfc, true, native_endian(), get_dim_sfc(sfc, dim), precision);
	std::vector<GEOSGeom> g(sfc.size());
	GEOSWKBReader *wkb_reader = GEOSWKBReader_create_r(hGEOSCtxt);
	for (int i = 0; i < sfc.size(); i++) {
		Rcpp::RawVector r = wkblst[i];
		g[i] = GEOSWKBReader_read_r(hGEOSCtxt, wkb_reader, &(r[0]), r.size());
	}
	GEOSWKBReader_destroy_r(hGEOSCtxt, wkb_reader);
	return g;
}

Rcpp::List sfc_from_geometry(GEOSContextHandle_t hGEOSCtxt, std::vector<GEOSGeom> geom, int dim = 2) {

	Rcpp::List out(geom.size());
	GEOSWKBWriter *wkb_writer = GEOSWKBWriter_create_r(hGEOSCtxt);
	GEOSWKBWriter_setOutputDimension_r(hGEOSCtxt, wkb_writer, dim);
	for (size_t i = 0; i < geom.size(); i++) {
		size_t size;
		unsigned char *buf = GEOSWKBWriter_write_r(hGEOSCtxt, wkb_writer, geom[i], &size);
		Rcpp::RawVector raw(size);
		memcpy(&(raw[0]), buf, size);
		GEOSFree_r(hGEOSCtxt, buf);
		out[i] = raw;
		GEOSGeom_destroy_r(hGEOSCtxt, geom[i]);
	}
	GEOSWKBWriter_destroy_r(hGEOSCtxt, wkb_writer);
	return CPL_read_wkb(out, true, false, native_endian());
}

Rcpp::NumericVector get_dim(double dim0, double dim1) {
	Rcpp::NumericVector dim(2);
	dim(0) = dim0;
	dim(1) = dim1;
	return dim;
}

Rcpp::IntegerVector get_which(Rcpp::LogicalVector row) {
	int j = 0;
	for (int i = 0; i < row.length(); i++)
		if (row(i))
			j++;
	Rcpp::IntegerVector ret(j);
	for (int i = 0, j = 0; i < row.length(); i++)
		if (row(i))
			ret(j++) = i + 1; // R is 1-based
	return ret;
}

bool chk_(char value) {
	if (value == 2)
		throw std::range_error("GEOS exception"); // #nocov
	return value; // 1: true, 0: false
}


typedef char (* log_fn)(GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *);
typedef char (* log_prfn)(GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *);

log_fn which_geom_fn(const std::string op) {
	if (op == "intersects")
		return GEOSIntersects_r;
	else if (op == "disjoint")
		return GEOSDisjoint_r;
	else if (op == "touches")
		return GEOSTouches_r;
	else if (op == "crosses")
		return GEOSCrosses_r;
	else if (op == "within")
		return GEOSWithin_r;
	else if (op == "contains")
		return GEOSContains_r;
	else if (op == "overlaps")
		return GEOSOverlaps_r;
	else if (op == "equals")
		return GEOSEquals_r;
	else if (op == "covers")
		return GEOSCovers_r;
	else if (op == "covered_by")
		return GEOSCoveredBy_r;
	throw std::range_error("wrong value for op"); // unlikely to happen unless user wants to
}

log_prfn which_prep_geom_fn(const std::string op) {
	if (op == "intersects")
		return GEOSPreparedIntersects_r;
	else if (op == "disjoint")
		return GEOSPreparedDisjoint_r;
	else if (op == "touches")
		return GEOSPreparedTouches_r;
	else if (op == "crosses")
		return GEOSPreparedCrosses_r;
	else if (op == "within")
		return GEOSPreparedWithin_r;
	else if (op == "contains")
		return GEOSPreparedContains_r;
	else if (op == "contains_properly") // not interfaced from R
		return GEOSPreparedContainsProperly_r;
	else if (op == "overlaps")
		return GEOSPreparedOverlaps_r;
	//else if (op == "equals")
	//	return GEOSPreparedEquals_r;
	else if (op == "covers")
		return GEOSPreparedCovers_r;
	else if (op == "covered_by")
		return GEOSPreparedCoveredBy_r;
	throw std::range_error("wrong value for op"); // unlikely to happen unless user wants to
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_binop(Rcpp::List sfc0, Rcpp::List sfc1, std::string op, double par = 0.0, 
		std::string pattern = "", bool sparse = true, bool prepared = false) {

	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();

	std::vector<GEOSGeom> gmv0 = geometries_from_sfc(hGEOSCtxt, sfc0, NULL);
	std::vector<GEOSGeom> gmv1 = geometries_from_sfc(hGEOSCtxt, sfc1, NULL);

	Rcpp::List ret_list;

	using namespace Rcpp; // so that later on the (i,_) works
	if (op == "relate") { // character return matrix:
		Rcpp::CharacterVector out(sfc0.length() * sfc1.length());
		for (int i = 0; i < sfc0.length(); i++) {
			for (int j = 0; j < sfc1.length(); j++) {
				char *cp = GEOSRelate_r(hGEOSCtxt, gmv0[i], gmv1[j]);
				if (cp == NULL)
					throw std::range_error("GEOS error in GEOSRelate_r"); // #nocov
				out[j * sfc0.length() + i] = cp;
				GEOSFree_r(hGEOSCtxt, cp);
			}
			R_CheckUserInterrupt();
		}
		out.attr("dim") = get_dim(sfc0.length(), sfc1.length());
		ret_list = Rcpp::List::create(out);
	} else if (op == "distance") { // return double matrix:
		Rcpp::NumericMatrix out(sfc0.length(), sfc1.length());
		for (size_t i = 0; i < gmv0.size(); i++) {
			for (size_t j = 0; j < gmv1.size(); j++) {
				double dist = -1.0;
				if (GEOSDistance_r(hGEOSCtxt, gmv0[i], gmv1[j], &dist) == 0)
					throw std::range_error("GEOS error in GEOSDistance_r"); // #nocov
				out(i,j) = dist;
			}
			R_CheckUserInterrupt();
		}
		ret_list = Rcpp::List::create(out);
	} else {
		// other cases: boolean return matrix, either dense or sparse
		Rcpp::LogicalMatrix densemat;
		if (! sparse)  // allocate:
			densemat = Rcpp::LogicalMatrix(sfc0.length(), sfc1.length());
		Rcpp::List sparsemat(sfc0.length());

		if (op == "equals_exact") { // has it's own signature, needing `par':
			for (int i = 0; i < sfc0.length(); i++) { // row
				Rcpp::LogicalVector rowi(sfc1.length()); 
				for (int j = 0; j < sfc1.length(); j++) 
					rowi(j) = chk_(GEOSEqualsExact_r(hGEOSCtxt, gmv0[i], gmv1[j], par));
				if (! sparse)
					densemat(i,_) = rowi;
				else
					sparsemat[i] = get_which(rowi);
				R_CheckUserInterrupt();
			}
		} else if (op == "relate_pattern") { // needing pattern
			for (int i = 0; i < sfc0.length(); i++) { // row
				Rcpp::LogicalVector rowi(sfc1.length()); 
				for (int j = 0; j < sfc1.length(); j++) 
					rowi(j) = chk_(GEOSRelatePattern_r(hGEOSCtxt, gmv0[i], gmv1[j], 
						pattern.c_str()));
				if (! sparse)
					densemat(i,_) = rowi;
				else
					sparsemat[i] = get_which(rowi);
				R_CheckUserInterrupt();
			}
		} else {
			if (prepared) {
				log_prfn logical_fn = which_prep_geom_fn(op);
				for (int i = 0; i < sfc0.length(); i++) { // row
					Rcpp::LogicalVector rowi(sfc1.length()); 
					const GEOSPreparedGeometry *pr = GEOSPrepare_r(hGEOSCtxt, gmv0[i]);
					for (int j = 0; j < sfc1.length(); j++)
						rowi(j) = chk_(logical_fn(hGEOSCtxt, pr, gmv1[j]));
					GEOSPreparedGeom_destroy_r(hGEOSCtxt, pr);
					if (! sparse)
						densemat(i,_) = rowi;
					else
						sparsemat[i] = get_which(rowi);
					R_CheckUserInterrupt();
				}
			} else {
				log_fn logical_fn = which_geom_fn(op);
				for (int i = 0; i < sfc0.length(); i++) { // row
					Rcpp::LogicalVector rowi(sfc1.length()); 
					for (int j = 0; j < sfc1.length(); j++)
						rowi(j) = chk_(logical_fn(hGEOSCtxt, gmv0[i], gmv1[j]));
					if (! sparse)
						densemat(i,_) = rowi;
					else
						sparsemat[i] = get_which(rowi);
					R_CheckUserInterrupt();
				}
			}
		}
		if (sparse)
			ret_list = sparsemat;
		else
			ret_list = Rcpp::List::create(densemat);
	}
	for (size_t i = 0; i < gmv0.size(); i++)
		GEOSGeom_destroy_r(hGEOSCtxt, gmv0[i]);
	for (size_t i = 0; i < gmv1.size(); i++)
		GEOSGeom_destroy_r(hGEOSCtxt, gmv1[i]);
	CPL_geos_finish(hGEOSCtxt);
	return ret_list;
}

// [[Rcpp::export]]
Rcpp::CharacterVector CPL_geos_is_valid_reason(Rcpp::List sfc) { 
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();

	std::vector<GEOSGeom> gmv = geometries_from_sfc(hGEOSCtxt, sfc, NULL);
	Rcpp::CharacterVector out(gmv.size());
	for (int i = 0; i < out.length(); i++) {
		char *buf = GEOSisValidReason_r(hGEOSCtxt, gmv[i]);
		if (buf == NULL)
			out[i] = NA_STRING;
		else {
			out[i] = buf;
			GEOSFree_r(hGEOSCtxt, buf);
		}
		GEOSGeom_destroy_r(hGEOSCtxt, gmv[i]);
	}
	CPL_geos_finish(hGEOSCtxt);
	return out;
}

// [[Rcpp::export]]
Rcpp::LogicalVector CPL_geos_is_valid(Rcpp::List sfc, bool NA_on_exception = true) { 
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();

	int notice = 0;
	if (NA_on_exception) {
		if (sfc.size() > 1)
			throw std::range_error("NA_on_exception will only work reliably with length 1 sfc objects");
#ifdef HAVE350
		GEOSContext_setNoticeMessageHandler_r(hGEOSCtxt, 
			(GEOSMessageHandler_r) __emptyNoticeHandler, (void *) &notice);
		GEOSContext_setErrorMessageHandler_r(hGEOSCtxt, 
			(GEOSMessageHandler_r) __countErrorHandler, (void *) &notice); 
#endif
	}

	std::vector<GEOSGeom> gmv = geometries_from_sfc(hGEOSCtxt, sfc, NULL); // where notice might be set!
#ifdef HAVE350
	GEOSContext_setNoticeHandler_r(hGEOSCtxt, __warningHandler);
	GEOSContext_setErrorHandler_r(hGEOSCtxt, __errorHandler);
#endif
	Rcpp::LogicalVector out(gmv.size());
	for (int i = 0; i < out.length(); i++) {
		int ret = GEOSisValid_r(hGEOSCtxt, gmv[i]);
		if (NA_on_exception && (ret == 2 || notice != 0))
			out[i] = NA_LOGICAL; // no need to set notice back here, as we only consider 1 geometry
		else
			out[i] = chk_(ret);
		GEOSGeom_destroy_r(hGEOSCtxt, gmv[i]);
	}
	CPL_geos_finish(hGEOSCtxt);
	return out;
}

// [[Rcpp::export]]
Rcpp::LogicalVector CPL_geos_is_simple(Rcpp::List sfc) { 
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();
	Rcpp::LogicalVector out(sfc.length());
	std::vector<GEOSGeom> g = geometries_from_sfc(hGEOSCtxt, sfc, NULL);
	for (size_t i = 0; i < g.size(); i++) {
		out[i] = chk_(GEOSisSimple_r(hGEOSCtxt, g[i]));
		GEOSGeom_destroy_r(hGEOSCtxt, g[i]);
	}
	CPL_geos_finish(hGEOSCtxt);
	return out;
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_union(Rcpp::List sfc, bool by_feature = false) { 
	int dim = 2;
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();
	std::vector<GEOSGeom> gmv = geometries_from_sfc(hGEOSCtxt, sfc, &dim);
	std::vector<GEOSGeom> gmv_out(by_feature ? sfc.size() : 1);
	if (by_feature) {
		for (int i = 0; i < sfc.size(); i++) {
			gmv_out[i] = GEOSUnaryUnion_r(hGEOSCtxt, gmv[i]);
			GEOSGeom_destroy_r(hGEOSCtxt, gmv[i]);
		}
	} else {
		GEOSGeom gc = GEOSGeom_createCollection_r(hGEOSCtxt, GEOS_GEOMETRYCOLLECTION, gmv.data(), gmv.size());
		gmv_out[0] = GEOSUnaryUnion_r(hGEOSCtxt, gc);
		GEOSGeom_destroy_r(hGEOSCtxt, gc);
	}

	Rcpp::List out(sfc_from_geometry(hGEOSCtxt, gmv_out, dim)); // destroys gmv_out
	CPL_geos_finish(hGEOSCtxt);
	out.attr("precision") = sfc.attr("precision");
	out.attr("crs") = sfc.attr("crs");
	return out;
}


GEOSGeometry *chkNULL(GEOSGeometry *value) {
	if (value == NULL)
		throw std::range_error("GEOS exception"); // #nocov
	R_CheckUserInterrupt();
	return value;
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_op(std::string op, Rcpp::List sfc, 
		Rcpp::NumericVector bufferDist, int nQuadSegs = 30,
		double dTolerance = 0.0, bool preserveTopology = false, 
		int bOnlyEdges = 1, double dfMaxLength = 0.0) {

	int dim = 2;
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init(); 

	std::vector<GEOSGeom> g = geometries_from_sfc(hGEOSCtxt, sfc, &dim);
	std::vector<GEOSGeom> out(sfc.length());

	if (op == "buffer") {
		if (bufferDist.size() != (int) g.size())
			throw std::invalid_argument("invalid dist argument"); // #nocov
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSBuffer_r(hGEOSCtxt, g[i], bufferDist[i], nQuadSegs));
	} else if (op == "boundary") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSBoundary_r(hGEOSCtxt, g[i]));
	} else if (op == "convex_hull") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSConvexHull_r(hGEOSCtxt, g[i]));
	} else if (op == "unary_union") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSUnaryUnion_r(hGEOSCtxt, g[i]));
	} else if (op == "simplify") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = preserveTopology ? chkNULL(GEOSTopologyPreserveSimplify_r(hGEOSCtxt, g[i], dTolerance)) :
					chkNULL(GEOSSimplify_r(hGEOSCtxt, g[i], dTolerance));
	} else if (op == "linemerge") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSLineMerge_r(hGEOSCtxt, g[i]));
	} else if (op == "polygonize") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSPolygonize_r(hGEOSCtxt, &(g[i]), 1));
	} else if (op == "centroid") {
		for (size_t i = 0; i < g.size(); i++) {
			out[i] = chkNULL(GEOSGetCentroid_r(hGEOSCtxt, g[i]));
		}
	} else
#if GEOS_VERSION_MAJOR >= 3 && GEOS_VERSION_MINOR >= 4
	if (op == "triangulate") {
		for (size_t i = 0; i < g.size(); i++)
			out[i] = chkNULL(GEOSDelaunayTriangulation_r(hGEOSCtxt, g[i], dTolerance, bOnlyEdges));
	} else
#endif
		throw std::invalid_argument("invalid operation"); // would leak g and out // #nocov

	for (size_t i = 0; i < g.size(); i++)
		GEOSGeom_destroy_r(hGEOSCtxt, g[i]);

	Rcpp::List ret(sfc_from_geometry(hGEOSCtxt, out, dim)); // destroys out
	CPL_geos_finish(hGEOSCtxt);
	ret.attr("precision") = sfc.attr("precision");
	ret.attr("crs") = sfc.attr("crs");
	return ret;
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_voronoi(Rcpp::List sfc, Rcpp::List env, double dTolerance = 0.0, int bOnlyEdges = 1) {

	int dim = 2;
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init(); 

	std::vector<GEOSGeom> g = geometries_from_sfc(hGEOSCtxt, sfc, &dim);
	std::vector<GEOSGeom> out(sfc.length());

#ifdef HAVE350
	switch (env.size()) {
		case 0: ;
		case 1: {
			std::vector<GEOSGeom> g_env = geometries_from_sfc(hGEOSCtxt, env);
			for (size_t i = 0; i < g.size(); i++) {
				out[i] = chkNULL(GEOSVoronoiDiagram_r(hGEOSCtxt, g[i], 
					g_env.size() ? g_env[0] : NULL, dTolerance, bOnlyEdges));
				GEOSGeom_destroy_r(hGEOSCtxt, g[i]);
			}
			if (g_env.size())
				GEOSGeom_destroy_r(hGEOSCtxt, g_env[0]);
			break;
		}
		default:
			throw std::invalid_argument("env should have length 0 or 1"); // #nocov
	}
#else
	throw std::invalid_argument("voronoi diagrams require a GEOS version >= 3.5.0"); // #nocov
#endif

	Rcpp::List ret(sfc_from_geometry(hGEOSCtxt, out, dim)); // destroys out
	CPL_geos_finish(hGEOSCtxt);
	ret.attr("precision") = sfc.attr("precision");
	ret.attr("crs") = sfc.attr("crs");
	return ret;
}

GEOSGeometry *chkNULLcnt(GEOSContextHandle_t hGEOSCtxt, GEOSGeometry *value, size_t *n) {
	if (value == NULL)
		throw std::range_error("GEOS exception"); // #nocov
	if (!chk_(GEOSisEmpty_r(hGEOSCtxt, value)))
		*n = *n + 1;
	R_CheckUserInterrupt();
	return value;
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_op2(std::string op, Rcpp::List sfcx, Rcpp::List sfcy) {

	int dim = 2;
	GEOSContextHandle_t hGEOSCtxt = CPL_geos_init();
	std::vector<GEOSGeom> x = geometries_from_sfc(hGEOSCtxt, sfcx, &dim);
	std::vector<GEOSGeom> y = geometries_from_sfc(hGEOSCtxt, sfcy, &dim);
	std::vector<GEOSGeom> out(x.size() * y.size());

	size_t n = 0;
	if (op == "intersection") {
		for (size_t i = 0; i < y.size(); i++) {
			for (size_t j = 0; j < x.size(); j++)
				out[i * x.size() + j] = chkNULLcnt(hGEOSCtxt, GEOSIntersection_r(hGEOSCtxt, x[j], y[i]), &n);
			R_CheckUserInterrupt();
		}
	} else if (op == "union") {
		for (size_t i = 0; i < y.size(); i++) {
			for (size_t j = 0; j < x.size(); j++)
				out[i * x.size() + j] = chkNULLcnt(hGEOSCtxt, GEOSUnion_r(hGEOSCtxt, x[j], y[i]), &n);
			R_CheckUserInterrupt();
		}
	} else if (op == "difference") {
		for (size_t i = 0; i < y.size(); i++) {
			for (size_t j = 0; j < x.size(); j++)
				out[i * x.size() + j] = chkNULLcnt(hGEOSCtxt, GEOSDifference_r(hGEOSCtxt, x[j], y[i]), &n);
			R_CheckUserInterrupt();
		}
	} else if (op == "sym_difference") {
		for (size_t i = 0; i < y.size(); i++) {
			for (size_t j = 0; j < x.size(); j++)
				out[i * x.size() + j] = chkNULLcnt(hGEOSCtxt, GEOSSymDifference_r(hGEOSCtxt, x[j], y[i]), &n);
			R_CheckUserInterrupt();
		}
	} else 
		throw std::invalid_argument("invalid operation"); // would leak g, g0 and out // #nocov
	// clean up x and y:
	for (size_t i = 0; i < x.size(); i++)
		GEOSGeom_destroy_r(hGEOSCtxt, x[i]);
	for (size_t i = 0; i < y.size(); i++)
		GEOSGeom_destroy_r(hGEOSCtxt, y[i]);
	// trim results back to non-empty geometries:
	std::vector<GEOSGeom> out2(n);
	Rcpp::NumericMatrix m(n, 2); // and a set of 1-based indices to x and y
	size_t k = 0, l = 0;
	for (size_t i = 0; i < y.size(); i++) {
		for (size_t j = 0; j < x.size(); j++) {
			l = i * x.size() + j;
			if (!chk_(GEOSisEmpty_r(hGEOSCtxt, out[l]))) { // keep:
				out2[k] = out[l];
				m(k, 0) = j + 1;
				m(k, 1) = i + 1;
				k++;
				if (k > n)
					throw std::range_error("invalid k"); // #nocov
			} else // discard:
				GEOSGeom_destroy_r(hGEOSCtxt, out[l]);
		}
	}
	if (k != n)
		throw std::range_error("invalid k, check 2"); // #nocov

	Rcpp::List ret(sfc_from_geometry(hGEOSCtxt, out2, dim)); // destroys out2
	CPL_geos_finish(hGEOSCtxt);
	ret.attr("crs") = sfcx.attr("crs");
	ret.attr("idx") = m;
	return ret;
}

// [[Rcpp::export]]
std::string CPL_geos_version(bool b = false) {
	return GEOS_VERSION;
}

// [[Rcpp::export]]
Rcpp::NumericMatrix CPL_geos_dist(Rcpp::List sfc0, Rcpp::List sfc1) {
	Rcpp::NumericMatrix out = CPL_geos_binop(sfc0, sfc1, "distance", 0.0, "", false)[0];
	return out;
}

// [[Rcpp::export]]
Rcpp::CharacterVector CPL_geos_relate(Rcpp::List sfc0, Rcpp::List sfc1) {
	Rcpp::CharacterVector out = CPL_geos_binop(sfc0, sfc1, "relate", 0.0, "", false)[0];
	return out;	
}

// [[Rcpp::export]]
Rcpp::List CPL_invert_sparse_incidence(Rcpp::List m, int n) {
// invert a sparse incidence matrix list m that has n columns
	std::vector<size_t> sizes(n);
	for (int i = 0; i < n; i++)
		sizes[i] = 0; // init
	for (int i = 0; i < m.size(); i++) {
		Rcpp::IntegerVector v = m[i];
		for (int j = 0; j < v.size(); j++) {
			if (v[j] > n || v[j] < 0)
				throw std::range_error("CPL_invert_sparse: index out of bounds");
			sizes[v[j] - 1] += 1; // count
		}
	}
	Rcpp::List out(n);
	for (int i = 0; i < n; i++)
		out[i] = Rcpp::IntegerVector(sizes[i]);
	for (int i = 0; i < m.size(); i++) {
		Rcpp::IntegerVector v = m[i];
		for (int j = 0; j < v.size(); j++) {
			size_t new_i = v[j] - 1;
			Rcpp::IntegerVector w = out[new_i];
			w[w.size() - sizes[new_i]] = i + 1; // 1-based
			sizes[new_i] -= 1;
		}
	}
	return out;
}
