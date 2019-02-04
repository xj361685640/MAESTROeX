module tagging_module

  use parallel, only: parallel_IOProcessor
  use meth_params_module, only: temp_comp, rho_comp, nscal
  use probin_module, only: tag_density_3
  use base_state_geometry_module, only: nr_fine, max_radial_level

  implicit none

  private

contains

  ! ::: -----------------------------------------------------------
  ! ::: This routine will tag high error cells based on the state
  ! :::
  ! ::: INPUTS/OUTPUTS:
  ! :::
  ! ::: tag        <=  integer tag array
  ! ::: tag_lo,hi   => index extent of tag array
  ! ::: state       => state array
  ! ::: state_lo,hi => index extent of state array
  ! ::: set         => integer value to tag cell for refinement
  ! ::: clear       => integer value to untag cell
  ! ::: lo,hi       => work region we are allowed to change
  ! ::: dx          => cell size
  ! ::: time        => problem evolution time
  ! ::: level       => refinement level of this array
  ! ::: -----------------------------------------------------------

  subroutine state_error(tag,tag_lo,tag_hi, &
       state,state_lo,state_hi, &
       set,clear,&
       lo,hi,&
       dx,time,tag_err) bind(C, name="state_error")

    integer          :: lo(3),hi(3)
    integer          :: state_lo(3),state_hi(3)
    integer          :: tag_lo(3),tag_hi(3)
    double precision :: state(state_lo(1):state_hi(1), &
         state_lo(2):state_hi(2), &
         state_lo(3):state_hi(3), 1:nscal)
    integer          :: tag(tag_lo(1):tag_hi(1),tag_lo(2):tag_hi(2),tag_lo(3):tag_hi(3))
    double precision :: dx(3),time
    double precision :: tag_err(2)
    integer          :: set,clear

    ! local
    integer          :: i, j, k
    double precision :: temperr, denserr


    ! set temperature and density flags
    temperr = tag_err(1)
    denserr = tag_err(2)

    ! Tag on regions of high temperature
    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)
             if (state(i,j,k,rho_comp) .ge. tag_density_3) then
                tag(i,j,k) = set
             endif
          enddo
       enddo
    enddo

  end subroutine state_error

  subroutine tag_boxes(tag,tag_lo,tag_hi, &
                        set,clear,&
                        lo,hi,&
                        dx,time,&
                        lev,tag_array) bind(C, name="tag_boxes")

    integer          :: lo(3),hi(3)
    integer          :: tag_lo(3),tag_hi(3)
    integer          :: tag(tag_lo(1):tag_hi(1),tag_lo(2):tag_hi(2),tag_lo(3):tag_hi(3))
    double precision :: dx(3),time
    integer          :: tag_array(0:max_radial_level,0:nr_fine-1)
    integer          :: set,clear,lev

    ! local
    integer          :: i, j, k, r

    if (parallel_IOProcessor()) then
       print*,"Create a local copy of tagging.F90 in your build directory"
       print*,"Here is a sample that tags the full grid using a predetermined tag array"
    end if

    ! abort program
    call bl_error()


    ! Tag on regions of high temperature
#if (AMREX_SPACEDIM == 3)
    do k = lo(3), hi(3)

       if (tag_array(lev,k) > 0) then
          do j = lo(2), hi(2)
             do i = lo(1), hi(1)
                tag(i,j,k) = set
             enddo
          enddo
       endif

    enddo

#elif (AMREX_SPACEDIM == 2)
    do k = lo(3), hi(3)
       do j = lo(2), hi(2)

          if (tag_array(lev,j) > 0) then
             do i = lo(1), hi(1)
                tag(i,j,k) = set
             enddo
          endif

       enddo
    enddo

#else
    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             if (tag_array(lev,i) > 0) then
                tag(i,j,k) = set
             endif

          enddo
       enddo
    enddo

#endif

  end subroutine tag_boxes

  subroutine retag_array(set,clear,&
                          lo,hi,&
                          lev,tag_array) bind(C, name="retag_array")

    integer          :: lo(3),hi(3)
    integer          :: tag_array(0:max_radial_level,0:nr_fine-1)
    integer          :: set,clear,lev

    ! local
    integer          :: i, j, k, r


    if (parallel_IOProcessor()) then
       print*,"Create a local copy of tagging.F90 in your build directory"
       print*,"Here is a sample that tags the refined region including buffer zone"
    end if

    ! abort program
    call bl_error()

    ! Tag on regions including buffer cells
    do k = lo(3), hi(3)
    do j = lo(2), hi(2)
    do i = lo(1), hi(1)

#if (AMREX_SPACEDIM == 3)
          r = k/2
#elif (AMREX_SPACEDIM == 2)
          r = j/2
#else
          r = i/2
#endif
          tag_array(lev-1,r) = set
    enddo
    enddo
    enddo

  end subroutine retag_array

end module tagging_module
