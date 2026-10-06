import { el } from "./dom";

export type CameraPanel = {
  element: HTMLElement;
  start(width: number, height: number, onFrame: (frame: ImageBitmap) => void, onError: (message: string) => void): void;
  taken(): void;
  show(frame: ImageBitmap): void;
  stop(): void;
  hide(): void;
};

// Turns a refusal of the browser into what the person can do about it.
function describeCameraError(error: unknown): string {
  const name = error instanceof DOMException ? error.name : "";
  if (name === "NotAllowedError" || name === "SecurityError") {
    return "The camera permission was refused. Allow the camera for this page in the browser and run the example again.";
  }
  if (name === "NotFoundError" || name === "OverconstrainedError") {
    return "No camera was found. Connect a camera and run the example again.";
  }
  if (name === "NotReadableError" || name === "AbortError") {
    return "The camera could not start, because another app may be using it.";
  }
  const reason = error instanceof Error ? error.message : String(error);
  return `The camera could not start: ${reason}`;
}

// The live camera beside the picture Lua made from it, which a run opens through `host.cameraStart`.
export function mountCameraPanel(parent: HTMLElement): CameraPanel {
  const element = el(
    "section",
    "hidden flex-col gap-3 rounded-2xl border border-zinc-800/80 bg-zinc-900/40 p-4 shadow-xl shadow-black/40 backdrop-blur",
  );
  element.appendChild(el("h2", "text-sm font-medium text-zinc-300", "Camera"));

  const views = el("div", "grid gap-4 sm:grid-cols-2");
  const figure = (caption: string, media: HTMLElement) => {
    const box = el("figure", "flex flex-col gap-2");
    media.className = "aspect-[4/3] w-full rounded-xl border border-zinc-800 bg-black object-cover";
    box.appendChild(media);
    box.appendChild(el("figcaption", "text-xs text-zinc-500", caption));
    return box;
  };

  const video = document.createElement("video");
  video.muted = true;
  video.playsInline = true;
  const canvas = document.createElement("canvas");
  const picture = canvas.getContext("bitmaprenderer");
  views.appendChild(figure("The camera, as the browser delivers it.", video));
  views.appendChild(figure("The same frame after Lua changed its bytes in place.", canvas));
  element.appendChild(views);

  const status = el("p", "text-xs text-zinc-500", "");
  element.appendChild(status);
  parent.appendChild(element);

  let stream: MediaStream | null = null;
  let session = 0;
  let waiting = false;

  const stop = () => {
    session++;
    waiting = false;
    stream?.getTracks().forEach((track) => track.stop());
    stream = null;
    video.srcObject = null;
  };

  const fail = (message: string, onError: (message: string) => void) => {
    stop();
    status.textContent = message;
    status.className = "text-xs text-red-400";
    onError(message);
  };

  const start = async (width: number, height: number, onFrame: (frame: ImageBitmap) => void, onError: (message: string) => void) => {
    stop();
    const current = session;
    canvas.width = width;
    canvas.height = height;
    element.classList.replace("hidden", "flex");
    status.className = "text-xs text-zinc-500";
    status.textContent = "Waiting for the camera permission.";

    if (!navigator.mediaDevices?.getUserMedia) {
      fail("The camera needs a secure page, served over HTTPS or from localhost, in a browser that offers it.", onError);
      return;
    }

    try {
      const opened = await navigator.mediaDevices.getUserMedia({ video: { width: { ideal: 640 }, height: { ideal: 480 } }, audio: false });
      if (current !== session) {
        opened.getTracks().forEach((track) => track.stop());
        return;
      }
      stream = opened;
      opened.getVideoTracks().forEach((track) =>
        track.addEventListener("ended", () => {
          if (current === session) {
            fail("The camera stopped, because it was disconnected or turned off.", onError);
          }
        }),
      );
      video.srcObject = opened;
      await video.play();
    } catch (error) {
      if (current === session) {
        fail(describeCameraError(error), onError);
      }
      return;
    }

    status.textContent = `The camera is live, and Lua turns each frame of ${width} by ${height} pixels black and white.`;

    // A frame goes to the worker only once it took the one before, and the center of the camera is cut to the shape Lua asked for.
    const pump = async () => {
      if (current !== session) {
        return;
      }

      if (!waiting && video.videoWidth > 0) {
        const scale = Math.min(video.videoWidth / width, video.videoHeight / height);
        const cropWidth = width * scale;
        const cropHeight = height * scale;
        waiting = true;
        try {
          const frame = await createImageBitmap(
            video,
            (video.videoWidth - cropWidth) / 2,
            (video.videoHeight - cropHeight) / 2,
            cropWidth,
            cropHeight,
            { resizeWidth: width, resizeHeight: height, resizeQuality: "low" },
          );
          if (current === session) {
            onFrame(frame);
          } else {
            frame.close();
          }
        } catch (error) {
          waiting = false;
          if (current === session) {
            fail(describeCameraError(error), onError);
          }
          return;
        }
      }

      video.requestVideoFrameCallback(() => void pump());
    };
    void pump();
  };

  return {
    element,
    start: (width, height, onFrame, onError) => void start(width, height, onFrame, onError),
    taken: () => {
      waiting = false;
    },
    show: (frame) => {
      if (picture) {
        picture.transferFromImageBitmap(frame);
      } else {
        frame.close();
      }
    },
    stop: () => {
      if (stream) {
        status.className = "text-xs text-zinc-500";
        status.textContent = "The camera stopped with the run.";
      }
      stop();
    },
    hide: () => {
      stop();
      element.classList.replace("flex", "hidden");
    },
  };
}
