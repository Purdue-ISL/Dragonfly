using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;

public class LookAround : MonoBehaviour
{
    // Update is called once per frame

    StreamWriter logfile;
    void Update()
    {
        float pitch = 90;
        float x_transform = transform.localRotation.eulerAngles.x;
        if(x_transform <= 360 && x_transform >= 270)
        {
            pitch = x_transform - 270;
        }
        else if(x_transform <= 90 && x_transform >= 0)
        {
            pitch = x_transform + 90;
        }

        // Debug.LogFormat("X:{0} Y:{1} Z:{2}", pitch, transform.localRotation.eulerAngles.y, transform.localRotation.eulerAngles.z);
    }
}
