using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;

public class MouseLook : MonoBehaviour
{

    public InputActionReference horizontalLook;
    public InputActionReference verticalLook;
    public float lookSpeed = 0.5f;
    public Transform cameraTransform;
    float pitch;
    float yaw;
    // Start is called before the first frame update
    void Start()
    {
        Cursor.lockState = CursorLockMode.Locked;
        horizontalLook.action.performed += HandleHorizontalLookChange;
        // verticalLook.action.performed += HandleVerticalLookChange;
    }

    void HandleHorizontalLookChange(InputAction.CallbackContext obj)
    {
        yaw += obj.ReadValue<float>();
        transform.localRotation = Quaternion.AngleAxis(yaw * lookSpeed, Vector3.up);
    }

    void HandleVerticalLookChange(InputAction.CallbackContext obj)
    {
        pitch -= obj.ReadValue<float>();
        transform.localRotation = Quaternion.AngleAxis(pitch * lookSpeed, Vector3.right);

    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
