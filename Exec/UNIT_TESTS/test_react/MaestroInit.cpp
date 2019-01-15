
#include <Maestro.H>
#include <AMReX_VisMF.H>
using namespace amrex;


// initialize AMR data
// perform initial projection
// perform divu iters
// perform initial (pressure) iterations
void
Maestro::Init ()
{
	// timer for profiling
	BL_PROFILE_VAR("Maestro::Init()",Init);

	Print() << "Calling Init()" << std::endl;

	start_step = 1;

	// fill in multifab and base state data
	InitData();

	// set finest_radial_level in fortran
	// compute numdisjointchunks, r_start_coord, r_end_coord
	init_multilevel(tag_array.dataPtr(),&finest_level);

	// compute initial time step
	FirstDt();

	if (stop_time >= 0. && t_old+dt > stop_time) {
		dt = std::min(dt,stop_time-t_old);
		Print() << "Stop time limits dt = " << dt << std::endl;
	}

	dtold = dt;
	t_new = t_old + dt;

}

// fill in multifab and base state data
void
Maestro::InitData ()
{
	// timer for profiling
	BL_PROFILE_VAR("Maestro::InitData()",InitData);

	Print() << "Calling InitData()" << std::endl;

	// read in model file and fill in s0_init and p0_init for all levels

	init_base_state(s0_init.dataPtr(),p0_init.dataPtr(),rho0_old.dataPtr(),
	                rhoh0_old.dataPtr(),p0_old.dataPtr(),tempbar.dataPtr(),
	                tempbar_init.dataPtr());

	// calls AmrCore::InitFromScratch(), which calls a MakeNewGrids() function
	// that repeatedly calls Maestro::MakeNewLevelFromScratch() to build and initialize
	InitFromScratch(t_old);

	// reset tagging array to include buffer zones
	TagArray();

	// set finest_radial_level in fortran
	// compute numdisjointchunks, r_start_coord, r_end_coord
	init_multilevel(tag_array.dataPtr(),&finest_level);

	// average down data and fill ghost cells
	AverageDown(sold,0,Nscal);
	FillPatch(t_old,sold,sold,sold,0,0,Nscal,0,bcs_s);

	// free memory in s0_init and p0_init by swapping it
	// with an empty vector that will go out of scope
	Vector<Real> s0_swap, p0_swap;
	std::swap(s0_swap,s0_init);
	std::swap(p0_swap,p0_init);

	if (do_smallscale) {
		// set rho0_old = rhoh0_old = 0.
		std::fill(rho0_old.begin(),  rho0_old.end(),  0.);
		std::fill(rhoh0_old.begin(), rhoh0_old.end(), 0.);
	}
	else {
		// set rho0 to be the average
		Average(sold,rho0_old,Rho);

		// call eos with r,p as input to recompute T,h
		TfromRhoP(sold,p0_old,1);

		// set rhoh0 to be the average
		Average(sold,rhoh0_old,RhoH);
	}

	// set tempbar to be the average
	Average(sold,tempbar,Temp);
	for (int i=0; i<tempbar.size(); ++i) {
		tempbar_init[i] = tempbar[i];
	}

	// set p0^{-1} = p0_old
	for (int i=0; i<p0_old.size(); ++i) {
		p0_nm1[i] = p0_old[i];
	}

}

// During initialization of a simulation, Maestro::InitData() calls
// AmrCore::InitFromScratch(), which calls
// a MakeNewGrids() function that repeatedly calls this function to build
// and initialize finer levels.  This function creates a new fine
// level that did not exist before by interpolating from the coarser level
// overrides the pure virtual function in AmrCore
void Maestro::MakeNewLevelFromScratch (int lev, Real time, const BoxArray& ba,
                                       const DistributionMapping& dm)
{
	// timer for profiling
	BL_PROFILE_VAR("Maestro::MakeNewLevelFromScratch()",MakeNewLevelFromScratch);

	sold              [lev].define(ba, dm,          Nscal, ng_s);
	snew              [lev].define(ba, dm,          Nscal, ng_s);
	uold              [lev].define(ba, dm, AMREX_SPACEDIM, ng_s);

	sold              [lev].setVal(0.);
	snew              [lev].setVal(0.);

	const Real* dx = geom[lev].CellSize();
	const Real* dx_fine = geom[max_level].CellSize();

	MultiFab& scal = sold[lev];
	MultiFab& vel = uold[lev];

	// Loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
	for (MFIter mfi(scal, true); mfi.isValid(); ++mfi)
	{
		const Box& tilebox = mfi.tilebox();
		const int* lo  = tilebox.loVect();
		const int* hi  = tilebox.hiVect();

		initdata(&lev, &t_old, ARLIM_3D(lo), ARLIM_3D(hi),
		         BL_TO_FORTRAN_FAB(scal[mfi]),
		         BL_TO_FORTRAN_FAB(vel[mfi]),
		         s0_init.dataPtr(), p0_init.dataPtr(),
		         ZFILL(dx));
	}

	if (lev > 0 && do_reflux) {
		flux_reg_s[lev].reset(new FluxRegister(ba, dm, refRatio(lev-1), lev, Nscal));
		flux_reg_u[lev].reset(new FluxRegister(ba, dm, refRatio(lev-1), lev, AMREX_SPACEDIM));
	}
}
