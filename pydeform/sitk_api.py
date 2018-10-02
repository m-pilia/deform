import SimpleITK as sitk
import pydeform
from . import interruptible


def register(
        fixed_images,
        moving_images,
        *,
        fixed_landmarks = None,
        moving_landmarks = None,
        initial_displacement = None,
        constraint_mask = None,
        constraint_values = None,
        settings = None,
        log = None,
        silent = False,
        num_threads = 0,
        subprocess = False,
        use_gpu = False,
        ):
    R""" Perform deformable registration.

    Parameters
    ----------
    fixed_images: Union[sitk.Image, List[sitk.Image]]
        Fixed image, or list of fixed images.

    moving_images: Union[sitk.Image, List[sitk.Image]]
        Moving image, or list of moving images.

    fixed_landmarks: np.ndarray
        Array of shape :math:`n \times 3`, containing
        `n` landmarks with (x, y, z) coordinates in
        image space. Requires to provide `moving_landmarks`.

    moving_landmarks: np.ndarray
        Array of shape :math:`n \times 3`, containing
        `n` landmarks with (x, y, z) coordinates in
        image space. Requires to provide `fixed_landmarks`.

    initial_displacement: sitk.Image
        Initial guess of the displacement field.

    constraint_mask: sitk.Image
        Boolean mask for the constraints on the displacement.
        Requires to provide `constraint_values`.

    constraint_values: sitk.Image
        Value for the constraints on the displacement.
        Requires to provide `constraint_mask`.

    settings: dict
        Python dictionary containing the settings for the
        registration.

    log: Union[StringIO, str]
        Output for the log, either a StringIO or a filename.

    silent: bool
        If `True`, do not write output to screen.

    num_threads: int
        Number of OpenMP threads to be used. If zero, the
        number is selected automatically.

    subprocess: bool
        If `True`, run the call in a subprocess and handle
        keyboard interrupts. This has a memory overhead, since
        a new instance of the intepreter is spawned and
        input objects are copied in the subprocess memory.

    use_gpu: bool
        If `True`, use GPU acceleration from a CUDA device.
        Requires a build with CUDA support.

    Returns
    -------
    sitk.Image
        Vector field containing the displacement that
        warps the moving image(s) toward the fixed image(s).
        The displacement is defined in the reference coordinates
        of the fixed image(s), and each voxel contains the
        displacement that allows to resample the voxel from the
        moving image(s).
    """

    if not isinstance(fixed_images, list):
        fixed_images = [fixed_images]
    if not isinstance(moving_images, list):
        moving_images = [moving_images]

    fixed_origin = fixed_images[0].GetOrigin()
    fixed_spacing = fixed_images[0].GetSpacing()
    moving_origin = moving_images[0].GetOrigin()
    moving_spacing = moving_images[0].GetSpacing()

    # Get numpy view of the input
    fixed_images = [sitk.GetArrayViewFromImage(img) for img in fixed_images]
    moving_images = [sitk.GetArrayViewFromImage(img) for img in moving_images]

    if initial_displacement:
        initial_displacement = sitk.GetArrayViewFromImage(initial_displacement)
    if constraint_mask:
        constraint_mask = sitk.GetArrayViewFromImage(constraint_mask)
    if constraint_values:
        constraint_values = sitk.GetArrayViewFromImage(constraint_values)

    register = interruptible.register if subprocess else pydeform.register

    # Perform registration through the numpy API
    displacement = register(fixed_images=fixed_images,
                            moving_images=moving_images,
                            fixed_origin=fixed_origin,
                            moving_origin=moving_origin,
                            fixed_spacing=fixed_spacing,
                            moving_spacing=moving_spacing,
                            fixed_landmarks=fixed_landmarks,
                            moving_landmarks=moving_landmarks,
                            initial_displacement=initial_displacement,
                            constraint_mask=constraint_mask,
                            constraint_values=constraint_values,
                            settings=settings,
                            log=log,
                            silent=silent,
                            num_threads=num_threads,
                            use_gpu=use_gpu,
                            )

    # Convert the result to SimpleITK
    displacement = sitk.GetImageFromArray(displacement)
    displacement.SetOrigin(fixed_origin)
    displacement.SetSpacing(fixed_spacing)

    return displacement

