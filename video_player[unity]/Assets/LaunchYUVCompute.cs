using System;
using System.IO;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using System.Threading;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Collections;
using System.Collections.Generic;

public class LaunchYUVCompute : MonoBehaviour
{
    [SerializeField] private ComputeShader shader;
    private ComputeBuffer buffer;
    private ComputeBuffer UVbufferA;
    private ComputeBuffer UVbufferB;
    private ComputeBuffer UVbufferC;
    private ComputeBuffer UVbufferD;

    private Socket _clientSocket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    private byte[] _recieveBuffer = new byte[4000000];

    float[] pitches;
    float[] yaws;

    byte[] first;
    byte[] second;
    int socket_counter;

    float headset_pitch;
    float headset_yaw;

    string log_path;

    [SerializeField] private int width, height;

    private Material mat;
    private RenderTexture tex;

    private int[] bytes;
    int num;
    int raw_pitch;
    int raw_yaw;

    int write_pitch;
    int write_yaw;
    float write_time;
    int left;
    int right;
    int top;
    int bottom;
    int m_width;
    int m_height;
    int frame_count;
    string prev_write;
    string directory_name;
    string directory_name1;

    int flag;
    string path;

    StreamWriter sw;
    string error_log_path;

    StreamWriter cw;
    string coordinate_log_path;

    void Start()
    {
        // string[] lines = File.ReadAllLines("C:/Users/Jagan/Documents/Source-360/V360System-ISL/build/isl_client_files/vp_corr_per_frame_user_3.txt");
        directory_name = "C:/Test/skip-system/system/yuvframes/";
        directory_name1 = "C:/Test/skip-system/system/";

        frame_count = 0;
        m_width = 1280;
        m_height = 1280;
        bytes = new int[11059200];
        buffer = new ComputeBuffer(bytes.Length, sizeof(int));
        num = 1;

        error_log_path = "C:/Test/skip-system/system/unity_render_frame_log.txt";
        sw = File.CreateText(error_log_path);

        coordinate_log_path = "C:/Test/skip-system/system/unity_coordinate_log.txt";
        cw = File.CreateText(coordinate_log_path);

        string posDataLog = String.Format("{0},{1},{2}\n", "Time", "Yaw", "Pitch");

        cw.WriteLine(posDataLog);

        pitches = new float[1500];
        yaws = new float[1500];
        int i = 0;
        // foreach(string line in lines)
        // {
        //     yaws[i] = float.Parse(line.Substring(0, line.IndexOf(',')));
        //     pitches[i] = float.Parse(line.Substring(line.IndexOf(',') + 1));
        //     i += 1;
        // }

        socket_counter = 0;
        first = new byte[0];
        SetupServer();

        UVbufferA = PrecomputeBufferValues(1280, 1120, UVbufferA, "UVBufferA", "YUVtoRGBA");
        UVbufferB = PrecomputeBufferValues(1280, 1280, UVbufferB, "UVBufferB", "YUVtoRGBB");
        UVbufferC = PrecomputeBufferValues(1600, 1120, UVbufferC, "UVBufferC", "YUVtoRGBC");
        UVbufferD = PrecomputeBufferValues(1600, 1280, UVbufferD, "UVBufferD", "YUVtoRGBD");
        path = "";
        Initialize();

        //InvokeRepeating("UpdatePeriod", 0.0f, 0.04f);
        InvokeRepeating("UpdateCoords", 0.1f, 0.04f);
    }

    void UpdateCoords()
    {
        float raw_pitch = 90;
        float raw_yaw = 180;
        float y_transform = transform.localRotation.eulerAngles.y;
        float x_transform = transform.localRotation.eulerAngles.x;
        // Debug
        if (x_transform <= 360 && x_transform >= 270)
        {
            raw_pitch = x_transform - 270;
        }
        else if (x_transform <= 90 && x_transform >= 0)
        {
            raw_pitch = x_transform + 90;
        }

        if (y_transform >= 0 && y_transform < 180)
        {
            raw_yaw = y_transform + 180;
        }
        else if (y_transform >= 180 && y_transform <= 360)
        {
            raw_yaw = y_transform - 180;
        }
        raw_yaw -= 92;
        if (raw_yaw < 0)
        {
            raw_yaw = raw_yaw + 360;
        }

        raw_pitch = 180 - raw_pitch;
        if (raw_pitch > 130)
        {
            raw_pitch = 130;
        }

        if (raw_pitch < 50)
        {
            raw_pitch = 50;
        }

        write_pitch = (int)raw_pitch;
        write_yaw = (int)raw_yaw;
        write_time = Time.time;
        string yaw_s = "" + write_yaw;
        string pitch_s = "" + write_pitch;

        string posData = string.Format("{0},{1},{2}", DateTimeOffset.Now.ToUnixTimeMilliseconds(), pitch_s.PadLeft(3, '0'), yaw_s.PadLeft(3, '0'));
        print("Sending...");
        SendData(posData);
        print("Sent...");
        string posDataLog = String.Format("{0},{1},{2}\n", DateTimeOffset.Now.ToUnixTimeMilliseconds(), pitch_s.PadLeft(3, '0'), yaw_s.PadLeft(3, '0'));

        cw.WriteLine(posDataLog);

    }

    void GetHeadsetCoords()
    {
        float raw_pitch = 90;
        float raw_yaw = 180;
        float y_transform = transform.localRotation.eulerAngles.y;
        float x_transform = transform.localRotation.eulerAngles.x;
        if (x_transform <= 360 && x_transform >= 270)
        {
            raw_pitch = x_transform - 270;
        }
        else if (x_transform <= 90 && x_transform >= 0)
        {
            raw_pitch = x_transform + 90;
        }

        if (y_transform >= 0 && y_transform < 180)
        {
            raw_yaw = y_transform + 180;
        }
        else if (y_transform >= 180 && y_transform <= 360)
        {
            raw_yaw = y_transform - 180;
        }
        raw_yaw -= 92;
        if (raw_yaw < 0)
        {
            raw_yaw = raw_yaw + 360;
        }
        headset_pitch = raw_pitch;
        headset_yaw = raw_yaw;
    }

    int mod(int x, int m)
    {
        return (x % m + m) % m;
    }

    double mod_float(double x, double m)
    {
        return (x % m + m) % m;
    }


    void GetCoords(int pitcher, int yawer)
    {
        raw_pitch = (int)((180.0 - pitcher));
        raw_yaw = (int)(yawer);

        int pitch = 90;
        int yaw = 88;

        GetHeadsetCoords();

        // Debug - Uncomment the line below to enable the headset pitch adjustment
        // pitch = (int)(headset_pitch * 10.66);
        pitch = (int)(headset_pitch);
        yaw = (int)(headset_yaw);
        yaw = raw_yaw;
        pitch = raw_pitch;
        left = raw_yaw - 50;
        right = raw_yaw + 50;
        top = raw_pitch - 50;
        bottom = raw_pitch + 50;

        int holdL = -1 * (raw_yaw - (left - (mod(left, 30))));
        int holdR = yaw + ((right - (mod(right, 30)) + (mod(right, 30) != 0 ? 30 : 0)) - raw_yaw);


        left = yaw - (raw_yaw - (left - (mod(left, 30))));
        right = yaw + ((right - (mod(right, 30)) + (mod(right, 30) != 0 ? 30 : 0)) - raw_yaw);
        top = pitch - (raw_pitch - (top - (mod(top, 15))));
        //bottom = pitch + ((bottom - (mod(bottom, 15)) + 15) - raw_pitch);
        bottom = pitch + ((bottom - (mod(bottom, 15)) + (mod(bottom, 15) != 0 ? 15 : 0)) - raw_pitch);

        // Debug.LogFormat("Pitch:{0} Yaw:{1}", pitch, yaw);
        // Debug.LogFormat("FirstItem:{0}, Left:{1}, Right:{2}, Top:{3}, Bottom:{4}", 0, left, right, top, bottom);

        if (((right - left) / 30 != m_width / 320) || ((bottom - top) / 15 != m_height / 160))
        {
            Debug.LogFormat("Error: Expected Width: {0}, Actual Width: {1} ", (right - left) / 30, m_width / 320);
            // sw.WriteLine(String.Format("Error: Expected Width: {0}, Actual Width {1}, HoldL {2} ", (right - left)/30, m_width/320, holdL));
            // sw.WriteLine(String.Format("Error: Expected Height: {0}, Actual Height {1}, HoldR {2} ", (bottom - top)/15, m_height/160, holdR));
            // sw.WriteLine(String.Format("Frame:{0}, Left:{1}, Right:{2}, Top:{3}, Bottom:{4}, headset_yaw:{5} \n", frame_count, left, right, top, bottom, yaw));
        }

        if (left < 0)
        {
            left = (left + 360);
        }
        if (right >= 360)
        {
            right = (right - 360);
        }
        if (top < 0)
        {
            top = (top + 180);
        }
        if (bottom >= 180)
        {
            bottom = (bottom - 180);
        }

        left = (int)Math.Round(left * 10.666666666);
        right = (int)Math.Round(right * 10.666666666);
        top = (int)Math.Round(top * 10.666666666);
        bottom = (int)Math.Round(bottom * 10.666666666);

        //Debug
        // Debug.LogFormat("Frame:{0}, Left:{1}, Right:{2}, Top:{3}, Bottom:{4}", frame_count, left, right, top, bottom);
        string write_data = String.Format("Yaw: {0}, Pitch: {1}, Left:{2}, Right:{3}, Top:{4}, Bottom:{5}, Time:{6} \n", yaw, (180 - pitch), left, right, top, bottom, System.DateTime.Now.ToString("hh.mm.ss.ffffff"));
        sw.WriteLine(write_data);
    }

    void UpdatePeriod()
    {
        //frame_count += 1;
        //if (frame_count == 1476)
        //{
        //    frame_count = 1;
        //}

        //byte[] fpointer = null;
        //string load_path = "";
        //if (!Directory.Exists(directory_name))
        //{
        //    print("Directory does not exist");
        //    frame_count -= 1;
        //    return;
        //}
        //string[] fileEntries = Directory.GetFiles(directory_name);
        //foreach (string fileName in fileEntries)
        //{
        //    if (fileName.Contains(".yuv") && fileName.Contains('/' + (frame_count).ToString() + '_'))
        //    {
        //        load_path = fileName;
        //        break;
        //    }
        //}

        string load_path = path;
        byte[] fpointer = null;


        if (load_path.Equals(""))
        {
            print("returning here");
            //frame_count -= 1;
            return;
        }

        // Comment

        print("XXXXXX");
        print(load_path);
        //Edit
        Debug.LogFormat("Width: {0} and Height {1} and Path {2}", load_path.Substring(load_path.IndexOf("_") + 1, 4), load_path.Substring(load_path.IndexOf("X") + 1, 4), load_path);
        m_width = int.Parse(load_path.Substring(load_path.IndexOf("_") + 1, 4));
        m_height = int.Parse(load_path.Substring(load_path.IndexOf("X") + 1, 4));
        string main_name = load_path.Split('.')[0];
        //pitches[frame_count - 1] = (float)(int.Parse(main_name.Split('_')[2]));
        //yaws[frame_count - 1] = (float)(int.Parse(main_name.Split('_')[3]));
        int pitcher = (int)(int.Parse(main_name.Split('_')[2]));
        int yawer = (int)(int.Parse(main_name.Split('_')[3]));

        Debug.LogFormat("Loading from: {0}", load_path);
        string write_data = String.Format("Loading from: {0}", load_path);
        sw.WriteLine(write_data);

        int exception_count = 0;

        while (exception_count < 8)
        {
            try
            {
                fpointer = File.ReadAllBytes(load_path);
                exception_count = 10;
            }
            catch
            {
                print("Error: Sharing violation");
                exception_count += 1;
            }
        }

        if (exception_count != 10)
        {
            return;
        }

        fpointer[0] = 0;
        fpointer[1] = 0;
        fpointer[2] = 0;
        fpointer[3] = 0;
        GetCoords(pitcher, yawer);
        UpdateGPU(fpointer);
        DispatchGPU();

    }

    void UpdateGPU(byte[] file)
    {
        // System.Buffer.BlockCopy(file, 0, compactBytes, 0, file.Length);
        // int[] converted = compactBytes;
        buffer.SetData(file);

        // Buffers n texture

        // Variables
        shader.SetInt("w", width);
        shader.SetInt("h", height);
        shader.SetInt("m_w", m_width);
        shader.SetInt("m_h", m_height);
        shader.SetInt("Ysize", m_width * m_height);
        shader.SetInt("UorVsize", m_width * m_height / 4);
        shader.SetInt("left", left);
        shader.SetInt("right", right);
        shader.SetInt("top", top);
        shader.SetInt("bottom", bottom);

    }

    private void Initialize()
    {
        CreateTexture();

        mat = RenderSettings.skybox;
        mat.mainTexture = tex;

        UpdatePeriod();
    }

    private void CreateTexture()
    {
        tex = new RenderTexture(width, height, 0, DefaultFormat.HDR);
        tex.enableRandomWrite = true;
        tex.Create();
    }

    private ComputeBuffer PrecomputeBufferValues(int m_width, int m_height, ComputeBuffer UVbuffer, string buffname, string kernelID)
    {
        int[] uvdata = new int[m_width * m_height];
        int offset = 0;
        for (int y = 0; y < m_height; y += 2)
        {
            for (int x = 0; x < m_width; x += 2)
            {
                //offset++;
                int i = y * m_width + x;
                uvdata[i] = offset;
                uvdata[i + m_width] = offset;
                uvdata[i + 1] = offset;
                uvdata[i + m_width + 1] = offset;
                offset++;
            }
        }

        UVbuffer = new ComputeBuffer(m_width * m_height, sizeof(int));
        UVbuffer.SetData(uvdata);
        shader.SetBuffer(shader.FindKernel(kernelID), buffname, UVbuffer);
        return UVbuffer;
    }

    void DispatchGPU()
    {
        int kernel_id = 0;
        if (m_width == 1280 && m_height == 1120)
        {
            kernel_id = shader.FindKernel("YUVtoRGBA");
            shader.SetBuffer(kernel_id, "ByteBuffer", buffer);
            shader.SetTexture(kernel_id, "Result", tex);
            shader.Dispatch(kernel_id, width / 8, height / 8, 1);
        }
        else if (m_width == 1280 && m_height == 1280)
        {
            kernel_id = shader.FindKernel("YUVtoRGBB");
            shader.SetBuffer(kernel_id, "ByteBuffer", buffer);
            shader.SetTexture(kernel_id, "Result", tex);
            shader.Dispatch(kernel_id, width / 8, height / 8, 1);
            //Debug.LogFormat("Using kernel {0}", shader.FindKernel("YUVtoRGBB"));
        }
        else if (m_width == 1600 && m_height == 1120)
        {
            kernel_id = shader.FindKernel("YUVtoRGBC");
            shader.SetBuffer(kernel_id, "ByteBuffer", buffer);
            shader.SetTexture(kernel_id, "Result", tex);
            shader.Dispatch(kernel_id, width / 8, height / 8, 1);
        }
        else if (m_width == 1600 && m_height == 1280)
        {
            kernel_id = shader.FindKernel("YUVtoRGBD");
            shader.SetBuffer(kernel_id, "ByteBuffer", buffer);
            shader.SetTexture(kernel_id, "Result", tex);
            shader.Dispatch(kernel_id, width / 8, height / 8, 1);
        }

    }

    private void OnDisable()
    {
        buffer.Release();
        UVbufferA.Release();
        UVbufferB.Release();
        UVbufferC.Release();
        UVbufferD.Release();
        // SaveTexture();
        //Directory.Delete(directory_name, true);
        sw.Close();
        cw.Close();
    }

    private void SetupServer()
    {
        try
        {
            _clientSocket.Connect(new IPEndPoint(IPAddress.Loopback, 6760));
        }
        catch (SocketException ex)
        {
            Debug.Log(ex.Message);
        }

        _clientSocket.BeginReceive(_recieveBuffer, 0, _recieveBuffer.Length, SocketFlags.None, new AsyncCallback(ReceiveCallback), null);
    }

    private void ReceiveCallback(IAsyncResult AR)
    {
        socket_counter += 1;
        //Check how much bytes are recieved and call EndRecieve to finalize handshake
        int recieved = _clientSocket.EndReceive(AR);

        if (recieved <= 0)
            return;

        //Copy the recieved data into new buffer , to avoid null bytes
        byte[] recData = new byte[recieved];
        Buffer.BlockCopy(_recieveBuffer, 0, recData, 0, recieved);


        string msg = Encoding.ASCII.GetString(recData);
        //Process data here the way you want , all your bytes will be stored in recData
        print(msg);
        // print("Recieved # of Bytes: " + recieved);

        if (msg.Contains("yuvframes"))
        {
            path = directory_name1 + msg;
            flag = 1;
        }

        //Start receiving again
        _clientSocket.BeginReceive(_recieveBuffer, 0, _recieveBuffer.Length, SocketFlags.None, new AsyncCallback(ReceiveCallback), null);
    }

    void Update()
    {
        if (flag == 1)
        {
            UpdatePeriod();
            flag = 0;
        }

    }

    private void SendData(string send_data)
    {
        byte[] data = Encoding.ASCII.GetBytes(send_data);
        SocketAsyncEventArgs socketAsyncData = new SocketAsyncEventArgs();
        socketAsyncData.SetBuffer(data, 0, data.Length);
        _clientSocket.SendAsync(socketAsyncData);
    }


    //Debugging Functions
    public void SaveTexture()
    {
        byte[] bytess = toTexture2D(tex).EncodeToPNG();
        System.IO.File.WriteAllBytes("C:/Users/Jagan/Documents/Source-360/tempi.png", bytess);
    }
    Texture2D toTexture2D(RenderTexture rTex)
    {
        Texture2D tex = new Texture2D(3840, 1920, TextureFormat.RGB24, false);
        RenderTexture.active = rTex;
        tex.ReadPixels(new Rect(0, 0, rTex.width, rTex.height), 0, 0);
        tex.Apply();
        return tex;
    }
}
