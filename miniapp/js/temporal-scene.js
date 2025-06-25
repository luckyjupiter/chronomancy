class TemporalScene {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.particles = null;
        this.spirals = null;
        this.animationId = null;
        
        this.init();
    }
    
    init() {
        // Scene setup
        this.scene = new THREE.Scene();
        
        // Camera setup
        this.camera = new THREE.PerspectiveCamera(
            75, 
            window.innerWidth / window.innerHeight, 
            0.1, 
            1000
        );
        this.camera.position.z = 50;
        
        // Renderer setup
        this.renderer = new THREE.WebGLRenderer({ 
            alpha: true, 
            antialias: true 
        });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.setClearColor(0x000000, 0);
        
        if (this.container) {
            this.container.appendChild(this.renderer.domElement);
        }
        
        this.createParticleField();
        this.createTemporalSpirals();
        this.createAmbientLights();
        
        // Start animation
        this.animate();
        
        // Handle window resize
        window.addEventListener('resize', () => this.onWindowResize());
    }
    
    createParticleField() {
        const particleGeometry = new THREE.BufferGeometry();
        const particleCount = 300; // Reduced from 1000 for better performance
        
        const positions = new Float32Array(particleCount * 3);
        const colors = new Float32Array(particleCount * 3);
        const sizes = new Float32Array(particleCount);
        
        const colorPalette = [
            new THREE.Color(0xFFD700), // Gold
            new THREE.Color(0x8B5CF6), // Purple
            new THREE.Color(0x06D6A0), // Cyan
            new THREE.Color(0xF8FAFC)  // White
        ];
        
        for (let i = 0; i < particleCount; i++) {
            // Random spherical distribution
            const radius = Math.random() * 200 + 50;
            const theta = Math.random() * Math.PI * 2;
            const phi = Math.random() * Math.PI;
            
            positions[i * 3] = radius * Math.sin(phi) * Math.cos(theta);
            positions[i * 3 + 1] = radius * Math.sin(phi) * Math.sin(theta);
            positions[i * 3 + 2] = radius * Math.cos(phi);
            
            const color = colorPalette[Math.floor(Math.random() * colorPalette.length)];
            colors[i * 3] = color.r;
            colors[i * 3 + 1] = color.g;
            colors[i * 3 + 2] = color.b;
            
            sizes[i] = Math.random() * 2 + 0.5;
        }
        
        particleGeometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        particleGeometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
        particleGeometry.setAttribute('size', new THREE.BufferAttribute(sizes, 1));
        
        const material = new THREE.ShaderMaterial({
            uniforms: {
                time: { value: 0.0 }
            },
            vertexShader: `
                attribute float size;
                attribute vec3 color;
                varying vec3 vColor;
                uniform float time;
                
                void main() {
                    vColor = color;
                    vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
                    
                    // Gentle pulsing effect
                    float pulse = sin(time * 0.002 + length(position) * 0.01) * 0.3 + 0.7;
                    
                    gl_PointSize = size * pulse * (300.0 / -mvPosition.z);
                    gl_Position = projectionMatrix * mvPosition;
                }
            `,
            fragmentShader: `
                varying vec3 vColor;
                
                void main() {
                    float r = distance(gl_PointCoord, vec2(0.5, 0.5));
                    if (r > 0.5) discard;
                    
                    float alpha = 1.0 - smoothstep(0.0, 0.5, r);
                    gl_FragColor = vec4(vColor, alpha * 0.8);
                }
            `,
            transparent: true,
            blending: THREE.AdditiveBlending,
            vertexColors: true
        });
        
        this.particles = new THREE.Points(particleGeometry, material);
        this.scene.add(this.particles);
    }
    
    createTemporalSpirals() {
        this.spirals = [];
        
        // Create three interweaving spirals
        for (let spiralIndex = 0; spiralIndex < 3; spiralIndex++) {
            const points = [];
            const colors = [];
            const colorPalette = [
                [1.0, 0.84, 0.0],  // Gold
                [0.54, 0.36, 0.96], // Purple
                [0.02, 0.84, 0.63]  // Cyan
            ];
            
            // Generate spiral points
            for (let i = 0; i <= 200; i++) {
                const angle = (i / 200) * Math.PI * 6 + (spiralIndex * Math.PI * 2 / 3);
                const radius = (i / 200) * 150;
                const height = Math.sin(angle * 2) * 20;
                
                const x = Math.cos(angle) * radius;
                const y = height;
                const z = Math.sin(angle) * radius;
                
                points.push(new THREE.Vector3(x, y, z));
                
                // Add color for this point
                const color = colorPalette[spiralIndex];
                colors.push(color[0], color[1], color[2]);
            }
            
            // Create geometry and material
            const geometry = new THREE.BufferGeometry().setFromPoints(points);
            geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
            
            const material = new THREE.LineBasicMaterial({
                vertexColors: true,
                transparent: true,
                opacity: 0.7,
                blending: THREE.AdditiveBlending
            });
            
            const spiral = new THREE.Line(geometry, material);
            this.spirals.push(spiral);
            this.scene.add(spiral);
        }
    }
    
    createAmbientLights() {
        // Ambient light
        const ambientLight = new THREE.AmbientLight(0x404080, 0.4);
        this.scene.add(ambientLight);
        
        // Directional light
        const directionalLight = new THREE.DirectionalLight(0xffd700, 0.6);
        directionalLight.position.set(1, 1, 1);
        this.scene.add(directionalLight);
        
        // Point lights for mystical effect
        const pointLight1 = new THREE.PointLight(0x8a2be2, 0.8, 100);
        pointLight1.position.set(20, 20, 20);
        this.scene.add(pointLight1);
        
        const pointLight2 = new THREE.PointLight(0x00bfff, 0.6, 80);
        pointLight2.position.set(-20, -20, -20);
        this.scene.add(pointLight2);
    }
    
    animate() {
        // Limit to 30fps for better performance
        setTimeout(() => {
            this.animationId = requestAnimationFrame(() => this.animate());
        }, 1000 / 30);
        
        if (!this.particles || !this.spirals) return;
        
        const time = Date.now() * 0.001;
        
        // Rotate particles slowly
        this.particles.rotation.y = time * 0.05;
        
        // Animate spirals
        this.spirals.forEach((spiral, index) => {
            spiral.rotation.z = time * (0.1 + index * 0.05);
            spiral.rotation.y = time * 0.02;
        });
        
        // Gentle camera movement
        this.camera.position.x = Math.sin(time * 0.1) * 10;
        this.camera.position.y = Math.cos(time * 0.08) * 5;
        this.camera.lookAt(0, 0, 0);
        
        this.renderer.render(this.scene, this.camera);
    }
    
    triggerAnomalyEffect() {
        // Create a burst effect when an anomaly is reported
        const burstGeometry = new THREE.SphereGeometry(1, 16, 16);
        const burstMaterial = new THREE.MeshBasicMaterial({
            color: 0xffd700,
            transparent: true,
            opacity: 0.8
        });
        
        const burst = new THREE.Mesh(burstGeometry, burstMaterial);
        this.scene.add(burst);
        
        // Animate the burst
        const startTime = Date.now();
        const animateBurst = () => {
            const elapsed = Date.now() - startTime;
            const progress = elapsed / 2000; // 2 second animation
            
            if (progress < 1) {
                burst.scale.setScalar(1 + progress * 20);
                burst.material.opacity = 0.8 * (1 - progress);
                requestAnimationFrame(animateBurst);
            } else {
                this.scene.remove(burst);
            }
        };
        
        animateBurst();
    }
    
    onWindowResize() {
        this.camera.aspect = window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(window.innerWidth, window.innerHeight);
    }
    
    destroy() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
        
        if (this.renderer && this.container) {
            this.container.removeChild(this.renderer.domElement);
        }
        
        // Clean up Three.js objects
        if (this.scene) {
            this.scene.clear();
        }
    }
}

// Initialize the temporal scene when the page loads
document.addEventListener('DOMContentLoaded', () => {
    window.temporalScene = new TemporalScene('three-canvas');
}); 