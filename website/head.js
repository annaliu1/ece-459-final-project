import * as THREE from "three";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";

let model;

// Scene
const scene = new THREE.Scene();

const container = document.getElementById("head-visualization");

// Camera
const camera = new THREE.PerspectiveCamera(
  75,
  container.clientWidth / container.clientHeight,
  0.1,
  1000
);

camera.position.z = 5;
camera.updateProjectionMatrix();

// Renderer
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(container.clientWidth, container.clientHeight);
renderer.setPixelRatio(window.devicePixelRatio);
container.appendChild(renderer.domElement);

// Ambient light (overall brightness)
const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
scene.add(ambientLight);

// Directional light (like the sun)
const dirLight = new THREE.DirectionalLight(0xffffff, 1);
dirLight.position.set(5, 10, 7.5);
scene.add(dirLight);

// Point light (emits light in all directions, like a bulb)
const pointLight = new THREE.PointLight(0xffffff, 1);
pointLight.position.set(0, 5, 5);
scene.add(pointLight);

// Load GLB model
const loader = new GLTFLoader();
loader.load(
  "mask.glb", // <-- your file path
  (gltf) => {
    model = gltf.scene;
    //if using mask glb
    model.scale.set(0.2, 0.2, 0.2);

    //if using head glb
    // model.scale.set(0.1, 0.1, 0.1);
    // model.rotation.x = -Math.PI / 2;

    const box = new THREE.Box3().setFromObject(model);
    const center = box.getCenter(new THREE.Vector3());

    model.position.x += model.position.x - center.x;
    model.position.y += model.position.y - center.y;
    model.position.z += model.position.z - center.z;

    scene.add(model);
  },
  (xhr) => {
    console.log((xhr.loaded / xhr.total) * 100 + "% loaded");
  },
  (error) => {
    console.error("Error loading model:", error);
  }
);

// loader.load("head.glb", (gltf) => {
//   model = gltf.scene;
//   scene.add(model);

//   // Once head loads, load mask
//   loader.load("mask.glb", (maskGltf) => {
//     maskModel = maskGltf.scene;

//     // Position mask relative to head
//     maskModel.position.set(0, 0.12, 0.15);
//     maskModel.rotation.set(0, Math.PI, 0);
//     maskModel.scale.set(0.9, 0.9, 0.9);

//     // Put the mask ON the head
//     model.add(maskModel);
//   });
// });

let targetRotationY = 0;

// Handle resizing
window.addEventListener("resize", () => {
  if (!container) return;
  camera.aspect = container.clientWidth / container.clientHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(container.clientWidth, container.clientHeight);
});

// Expose function to update head position
window.updateHeadModel = (position) => {
  if (!model) return;

  // Map positions to Y-axis rotation angles (in radians)
  const rotations = {
    "Extreme Right": THREE.MathUtils.degToRad(60),
    "Medium Right": THREE.MathUtils.degToRad(30),
    "Relatively Up": 0,
    "Medium Left": THREE.MathUtils.degToRad(-30),
    "Extreme Left": THREE.MathUtils.degToRad(-60),
  };

  targetRotationY = rotations[position] !== undefined ? rotations[position] : 0;
};

// Animation loop
function animate() {
  requestAnimationFrame(animate);

  if (model) {
    // model.rotation.y += 0.01; // rotate around Y axis
    // model.rotation.x += 0.005; // rotate around X axis (optional)
    // model.rotation.z += 0.01;

    // Smooth rotation based on head position data
    model.rotation.y += (targetRotationY - model.rotation.y) * 0.1;
  }

  renderer.render(scene, camera);
}
animate();
