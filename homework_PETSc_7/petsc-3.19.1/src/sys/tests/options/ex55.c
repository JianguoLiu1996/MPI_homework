
static char help[] = "Tests options database monitoring and precedence.\n\n";

#include <petscsys.h>
#include <petscviewer.h>

PetscErrorCode PetscOptionsMonitorCustom(const char name[], const char value[], PetscOptionSource source, void *ctx)
{
  PetscViewer viewer = (PetscViewer)ctx;

  PetscFunctionBegin;
  if (!value) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "* Removing option: %s\n", name));
  } else if (!value[0]) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "* Setting option: %s (no value)\n", name));
  } else {
    PetscCall(PetscViewerASCIIPrintf(viewer, "* Setting option: %s = %s\n", name, value));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  PetscViewer       viewer = NULL;
  PetscViewerFormat format;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, "ex55options", help));
  PetscCall(PetscOptionsInsertString(NULL, "-option1 1 -option2 -option3 value3"));
  PetscCall(PetscOptionsGetViewer(PETSC_COMM_WORLD, NULL, NULL, "-options_monitor_viewer", &viewer, &format, NULL));
  if (viewer) {
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(PetscOptionsMonitorSet(PetscOptionsMonitorCustom, viewer, NULL));
    PetscCall(PetscViewerPopFormat(viewer));
    PetscCall(PetscViewerDestroy(&viewer));
  }
  PetscCall(PetscOptionsInsertString(NULL, "-option4 value4 -option5"));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   testset:
      localrunfiles: ex55options .petscrc petscrc
      filter: grep -E -v -e "(options_left)"
      args: -options_left 0 -options_view -options_monitor_viewer ascii
      args: -skip_petscrc {{0 1}separate output} -options_monitor_cancel {{0 1}separate output}
      test:
        suffix: 1
      test:
        suffix: 2
        args: -options_monitor
      test:
        suffix: 3
        args: -options_monitor -option_cmd_1 option_cmd_1_val -option_cmd_2
   test:
      # test effect of -skip_petscrc in ex55options file
      suffix: 4
      localrunfiles: ex55options .petscrc petscrc
      filter: grep -E -v -e "(options_left)"
      args: -options_left 0 -options_view -options_monitor
   testset:
      # test -help / -help intro / -version from command line
      localrunfiles: ex55options .petscrc petscrc
      filter: grep -E -e "(version|help|^See)"
      args: -options_left -options_view -options_monitor
      test:
        suffix: 5a
        args: -help
      test:
        suffix: 5b
        args: -help intro
      test:
        suffix: 5c
        args: -version
   testset:
      # test -help / -help intro / -version from file
      localrunfiles: ex55options rc_help rc_help_intro rc_version
      filter: grep -E -e "(version|help|^See)"
      args: -skip_petscrc
      args: -options_left -options_view -options_monitor
      test:
        suffix: 6a
        args: -options_file rc_help
      test:
        suffix: 6b
        args: -options_file rc_help_intro
      test:
        suffix: 6c
        args: -options_file rc_version

   test:
     localrunfiles: ex55options .petscrc petscrc
     suffix: 7
     filter: grep -E -v -e "(options_left)"
     args: -options_monitor -options_left 0

TEST*/
