// https://stackoverflow.com/questions/22577457/update-data-on-a-page-without-refreshing
window.addEventListener('load', function () {
    var xhr_status = null;
    var xhr_title = null;
    var xhr_devicename = null;

    updateLiveData_Status = function () {
        xhr_status = new XMLHttpRequest();
        xhr_status.onreadystatechange = evenHandler_status;
        xhr_status.open("GET", "statusupdate.html", true);
        xhr_status.send(null);
    };

    const W = 128, H = 23, BYTES = W * H / 8;
    const canvas = document.getElementById('c');
    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(W, H);
    async function updateLiveData_Xbm() {
        try {
            const response = await fetch("xbm", { cache: "no-store" });
            if (!response.ok) throw new Error("HTTP-Fehler " + response.status);
            const buf = new Uint8Array(await response.arrayBuffer());

            for (let i = 0; i < W * H; i++) {
                const byte = buf[i >> 3], bit = i & 7;
                const on = (byte >> bit) & 1;
                const col = on ? 0 : 255;
                const p = i << 2;
                img.data[p] = img.data[p + 1] = img.data[p + 2] = col;
                img.data[p + 3] = 255;
            }

            ctx.putImageData(img, 0, 0);
        } catch (err) {
            console.error("Fehler beim Laden des XBM:", err);
        } finally {
            setTimeout(updateLiveData_Xbm, 1000); // Zyklus fortsetzen
        }
    }

    updateLiveData_Once = function () {
        xhr_title = new XMLHttpRequest();
        xhr_title.open("GET", "title", true);
        xhr_title.onreadystatechange = evenHandler_title;
        xhr_title.send(null);

        xhr_devicename = new XMLHttpRequest();
        xhr_devicename.onreadystatechange = evenHandler_devicename;
        xhr_devicename.open("GET", "devicename", true);
        xhr_devicename.send(null);
    };

    updateLiveData_Once();

    function evenHandler_status() {
        if (xhr_status.readyState == 4 && xhr_status.status == 200) {
            dataDiv = document.getElementById('status');
            dataDiv.innerHTML = xhr_status.responseText;
            window.setTimeout(updateLiveData_Status, 500);
        }
    }


    function evenHandler_title() {
        if (xhr_title.readyState == 4 && xhr_title.status == 200) {
            document.title = xhr_title.responseText;
            document.getElementById('title2').innerHTML = xhr_title.responseText;
        }
    }
    function evenHandler_devicename() {
        if (xhr_devicename.readyState == 4 && xhr_devicename.status == 200) {
            document.getElementById('devicename').innerHTML = xhr_devicename.responseText;
            window.setTimeout(updateLiveData_Status, 1);
            window.setTimeout(updateLiveData_Xbm, 100);
        }
    }
});